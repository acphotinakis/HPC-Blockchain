/**
 * @file pbft.cpp
 * @brief Implements the Practical Byzantine Fault Tolerance (PBFT) consensus
 * protocol.
 */
#include "../../include/sbmpi/consensus/pbft.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "../../include/sbmpi/consensus/pbft_messages.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/util/logging.h"

#include "../../include/sbmpi/util/metrics.h"
#include "../../include/sbmpi/util/timer.h"

#include <atomic>

namespace sbmpi
{
  namespace consensus
  {
    /**
     * @brief Implements the Practical Byzantine Fault Tolerance (PBFT)
     * consensus protocol.
     *
     * This class manages the PBFT phases (Pre-Prepare, Prepare, Commit) to
     * reach consensus on a microblock within a shard. It handles message
     * broadcasting, quorum checking, and fault tolerance calculations.
     */
    PBFT::PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes)
        : communicator(comm),
          myRank(rank),
          leaderRank(leaderRank),
          numNodes(numNodes)
    {
      // PBFT tolerance: f = (n-1)/3. Quorum = 2f + 1.
      maxFaultyNodes = (numNodes - 1) / 3;
    }

    /**
     * @brief Broadcasts a PBFT message to all other nodes in the communicator.
     * @param msg The PBFTMessage to be broadcast.
     */
    void PBFT::broadcastMessage(const PBFTMessage& msg)
    {
      std::vector<char> data = serializeMessage(msg);
      for (int i = 0; i < numNodes; ++i) {
        if (i != myRank) {
          network::send(data, i, 0, communicator);
        }
      }
    }

    /**
     * @brief Initiates the Pre-Prepare phase of PBFT.
     *
     * The leader node broadcasts the proposed block and a PRE-PREPARE message
     * to all replicas.
     * @param block The MicroBlock proposed by the leader.
     */
    void PBFT::prePrepare(const core::blocks::MicroBlock& block)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::PRE_PREPARE;
      msg.senderId  = myRank;
      msg.blockHash = block.getHash();

      // 1. Broadcast the full block first (simplification for simulation)
      std::vector<char> blockData = block.serialize();
      network::bcast(blockData, leaderRank, communicator);

      // 2. Broadcast the Pre-Prepare consensus message
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: Broadcasting PRE-PREPARE for block " +
                                      msg.blockHash);
      broadcastMessage(msg);
    }

    /**
     * @brief Sends a PREPARE message for a given block hash.
     *
     * Replicas send this message after receiving a valid PRE-PREPARE message
     * and verifying the proposed block.
     * @param blockHash The hash of the block being prepared.
     */
    void PBFT::prepare(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::PREPARE;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    /**
     * @brief Sends a COMMIT message for a given block hash.
     *
     * Nodes send this message after collecting a quorum of PREPARE messages.
     * @param blockHash The hash of the block being committed.
     */
    void PBFT::commit(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::COMMIT;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    /**
     * @brief Executes the PBFT consensus protocol to agree on a new microblock.
     *
     * This method orchestrates the entire PBFT process, including Pre-Prepare,
     * Prepare, and Commit phases. The leader proposes a block, and replicas
     * validate and agree upon it.
     *
     * @param transactions A vector of transactions to be included in the
     * proposed block.
     * @param previousHash The hash of the previous block in the blockchain.
     * @return The MicroBlock that has reached consensus.
     */
    std::pair<core::blocks::MicroBlock, int> PBFT::run(
      const std::vector<core::state::Transaction>& transactions,
      const std::string& previousHash, int runID)
    {
      core::blocks::MicroBlock block;
      int messagesExchanged = 0;

      // --- PHASE 0: PRE-PREPARE ---
      if (myRank == leaderRank) {
          util::Timer blockCreationTimer;
          blockCreationTimer.start();

          std::string merkleRoot = util::merkle(transactions);
          // Use the passed previousHash instead of the placeholder
          block.header = core::blocks::BlockHeader(1, previousHash, merkleRoot);
          block.transactions = transactions;

          blockCreationTimer.stop();
          double blockCreationTime = blockCreationTimer.elapsedSeconds();
          util::BlockMetrics::record("total_simulation", runID, block.getHash(),
                                    "Micro", transactions.size(),
                                    blockCreationTime, previousHash);

          util::Logger::getLogger().info("PBFT Leader: Proposing block with " +
                                        std::to_string(transactions.size()) +
                                        " transactions.");
          prePrepare(block);
          messagesExchanged +=
              (numNodes - 1) * 2;  // block broadcast + pre-prepare
      } else {
          // Replicas receive the block content
          std::vector<char> blockData;
          network::bcast(blockData, leaderRank, communicator);
          messagesExchanged++;
          block.deserialize(blockData);

          std::atomic<bool> allValid(true);

          // [STEP 1] Replica Verification Phase (Parallelized)
          // Replicas must independently verify the block content before voting
          // (PREPARE). This prevents a malicious leader from proposing invalid
          // blocks.
          int validCount   = 0;
          int invalidCount = 0;

  #pragma omp parallel for reduction(+ : validCount, invalidCount)
          for (size_t i = 0; i < block.transactions.size(); ++i) {
              if (block.transactions[i].verify()) {
                  validCount++;
                  util::Logger::getLogger().debug("Replica detected VERIFY in Tx: " +
                                                  block.transactions[i].id);
              } else {
                  invalidCount++;
                  util::Logger::getLogger().error("Replica detected FAULT in Tx: " +
                                                  block.transactions[i].id);
              }
          }

          // [STEP 2] Logging and Decision
          if (invalidCount > 0) {
              util::Logger::getLogger().error(
                  "PBFT Replica " + std::to_string(myRank) +
                  ": REJECTING block due to " + std::to_string(invalidCount) +
                  " faulty transactions.");

              // Abort consensus for this node by returning an empty result.
              // In a full production system, this would trigger a VIEW-CHANGE.
              return {{}, messagesExchanged};
          }

          util::Logger::getLogger().info(
              "PBFT Replica " + std::to_string(myRank) +
              ": Verified block proposal. (Valid: " + std::to_string(validCount) +
              ")");

          util::Logger::getLogger().debug(
              "PBFT Replica: Received and verified block proposal.");
      }

      std::string proposedBlockHash = block.getHash();
      int         quorum            = 2 * maxFaultyNodes + 1;

      // --- PHASE 1: PREPARE ---
      if (myRank != leaderRank) {
        prepare(proposedBlockHash);
        messagesExchanged += numNodes - 1;
      }

      int prepareCount = 0;
      std::set<int> prepareVoters;
      if (myRank != leaderRank) {
          prepareCount++;
          prepareVoters.insert(myRank);
      }

      while (prepareCount < quorum) {
          MPI_Status status;
          int flag = 0;
          MPI_Iprobe(MPI_ANY_SOURCE, 0, communicator, &flag, &status);
          
          if (flag) {
              int source = status.MPI_SOURCE;
              std::vector<char> msgData = network::recv(source, 0, communicator);
              messagesExchanged++;
              PBFTMessage msg = deserializeMessage(msgData);

              if (msg.type == PBFTMessageType::PREPARE &&
                  msg.blockHash == proposedBlockHash) {
                  if (prepareVoters.find(msg.senderId) == prepareVoters.end()) {
                      prepareVoters.insert(msg.senderId);
                      prepareCount++;
                  }
              }
          } else {
              // Sleep to prevent hanging
              std::this_thread::sleep_for(std::chrono::microseconds(100)); // 100µs
          }
      }
      
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                    "]: PREPARED (Quorum " +
                                    std::to_string(prepareCount) + ")");

      // --- PHASE 2: COMMIT ---
      commit(proposedBlockHash);
      messagesExchanged += numNodes - 1;

      int commitCount = 1;  // Start by counting myself
      std::set<int> commitVoters;
      commitVoters.insert(myRank);

      while (commitCount < quorum) {
          MPI_Status status;
          int flag = 0;
          MPI_Iprobe(MPI_ANY_SOURCE, 0, communicator, &flag, &status);
          
          if (flag) {
              int source = status.MPI_SOURCE;
              std::vector<char> msgData = network::recv(source, 0, communicator);
              messagesExchanged++;
              PBFTMessage msg = deserializeMessage(msgData);

              if (msg.type == PBFTMessageType::COMMIT &&
                  msg.blockHash == proposedBlockHash) {
                  if (commitVoters.find(msg.senderId) == commitVoters.end()) {
                      commitVoters.insert(msg.senderId);
                      commitCount++;
                  }
              }
          } else {
            // Sleep to prevent hanging
              std::this_thread::sleep_for(std::chrono::microseconds(100)); // 100µs
          }
      }
      
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                    "]: COMMITTED (Quorum " +
                                    std::to_string(commitCount) + ")");

      // Consensus reached, return block
      return {block, messagesExchanged};
    }

  }  // namespace consensus
}  // namespace sbmpi

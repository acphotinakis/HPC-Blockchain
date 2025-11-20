#include "../../include/sbmpi/consensus/pbft.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../include/sbmpi/consensus/pbft_messages.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/util/logging.h"  // Added logging

namespace sbmpi
{
  namespace consensus
  {

    PBFT::PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes)
        : communicator(comm),
          myRank(rank),
          leaderRank(leaderRank),
          numNodes(numNodes)
    {
      // PBFT tolerance: f = (n-1)/3. Quorum = 2f + 1.
      maxFaultyNodes = (numNodes - 1) / 3;
    }

    void PBFT::broadcastMessage(const PBFTMessage& msg)
    {
      std::vector<char> data = serializeMessage(msg);
      for (int i = 0; i < numNodes; ++i) {
        if (i != myRank) {
          network::send(data, i, 0, communicator);
        }
      }
    }

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

    void PBFT::prepare(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::PREPARE;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    void PBFT::commit(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::COMMIT;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    core::blocks::MicroBlock PBFT::run(
        const std::vector<core::state::Transaction>& transactions)
    {
      core::blocks::MicroBlock block;

      // --- PHASE 0: PRE-PREPARE ---
      if (myRank == leaderRank) {
        std::string merkleRoot = util::merkle(transactions);
        block.header = core::blocks::BlockHeader(1, "genesis_hash_placeholder",
                                                 merkleRoot);
        block.transactions = transactions;

        util::Logger::getLogger().info("PBFT Leader: Proposing block with " +
                                       std::to_string(transactions.size()) +
                                       " transactions.");
        prePrepare(block);
      } else {
        // Replicas receive the block content
        std::vector<char> blockData;
        network::bcast(blockData, leaderRank, communicator);
        block.deserialize(blockData);
        util::Logger::getLogger().debug(
            "PBFT Replica: Received block proposal.");
      }

      std::string proposedBlockHash = block.getHash();
      int         quorum            = 2 * maxFaultyNodes + 1;

      // --- PHASE 1: PREPARE ---
      if (myRank != leaderRank) {
        prepare(proposedBlockHash);
      }

      int prepareCount = 0;
      if (myRank == leaderRank)
        prepareCount++;
      else
        prepareCount++;

      std::set<int> prepareVoters;
      prepareVoters.insert(myRank);

      while (prepareCount < quorum) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int               source  = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage       msg     = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::PREPARE &&
            msg.blockHash == proposedBlockHash) {
          if (prepareVoters.find(msg.senderId) == prepareVoters.end()) {
            prepareVoters.insert(msg.senderId);
            prepareCount++;
          }
        }
      }
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: PREPARED (Quorum " +
                                      std::to_string(prepareCount) + ")");

      // --- PHASE 2: COMMIT ---
      commit(proposedBlockHash);

      int           commitCount = 1;
      std::set<int> commitVoters;
      commitVoters.insert(myRank);

      while (commitCount < quorum) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int               source  = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage       msg     = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::COMMIT &&
            msg.blockHash == proposedBlockHash) {
          if (commitVoters.find(msg.senderId) == commitVoters.end()) {
            commitVoters.insert(msg.senderId);
            commitCount++;
          }
        }
      }
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: COMMITTED (Quorum " +
                                      std::to_string(commitCount) + ")");

      // --- CONSENSUS REACHED ---
      return block;
    }

  }  // namespace consensus
}  // namespace sbmpi

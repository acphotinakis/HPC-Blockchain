/**
 * @file shard.cpp
 * @brief Implements the Shard class, representing a single shard in the
 * blockchain network.
 */
#include "../../include/sbmpi/network/shard.h"

#include <iostream>
#include <vector>

#include "../../include/sbmpi/consensus/pbft.h"
#include "../../include/sbmpi/core/blocks/micro_block.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/logging.h"
#include "../../include/sbmpi/util/serialization.h"
#include "mpi.h"

#include "../../include/sbmpi/util/metrics.h"
#include "../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace network
  {

    /**
     * @brief Represents a single shard in the sharded blockchain network.
     *
     * Each shard manages its own set of transactions, runs a PBFT consensus
     * protocol to produce microblocks, and reports these microblocks to the
     * Final Committee.
     */
    Shard::Shard(int id, MPI_Comm comm, int leaderRank)
        : id(id), communicator(comm), leaderRank(leaderRank)
    {
    }

    /**
     * @brief Destructor for the Shard class.
     */
    Shard::~Shard() {}

    /**
     * @brief Adds a transaction to the shard's local mempool.
     * @param tx The transaction to add.
     */
    void Shard::addTransaction(const core::state::Transaction& tx)
    {
      mempool.push_back(tx);
    }

    /**
     * @brief Returns the MPI communicator associated with this shard.
     * @return The MPI_Comm object for the shard.
     */
    MPI_Comm Shard::getCommunicator() const
    {
      return communicator;
    }

    /**
     * @brief Runs the PBFT consensus protocol within the shard.
     *
     * This method orchestrates the ingestion of transactions (for the shard
     * leader), the execution of the PBFT algorithm to agree on a microblock,
     * and the reporting of the finalized microblock to the Final Committee (by
     * the shard leader).
     * @return The MicroBlock that has been agreed upon by the shard.
     */
    core::blocks::MicroBlock Shard::runConsensus(int runID)
    {
      int my_shard_rank;
      int shard_size;
      MPI_Comm_rank(communicator, &my_shard_rank);
      MPI_Comm_size(communicator, &shard_size);

      // 1. INGESTION PHASE
      // If I am the Shard Leader (Rank 0 in this shard), I must receive the
      // workload from Root (Rank 0 in World)
      if (my_shard_rank == 0) {
        // Receive serialized transactions from Root Process (Global Rank 0) via
        // COMM_WORLD Note: We use MPI_COMM_WORLD because Root is likely not in
        // our shard communicator
        std::vector<char> buffer = network::recv(0, 0, MPI_COMM_WORLD);

        int offset = 0;
        int numTx  = util::unpack_int(buffer, offset);

        mempool.clear();
        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) + " Leader: Starting ingestion of " +
            std::to_string(numTx) + " transactions.");

        // [STEP 1] Deserialize Phase (Must be Serial)
        // We unpack all transactions into a temporary vector first.
        std::vector<core::state::Transaction> tempTxList;
        tempTxList.reserve(numTx);
        for (int i = 0; i < numTx; ++i) {
          int                      txSize = util::unpack_int(buffer, offset);
          std::vector<char>        txData(buffer.begin() + offset,
                                          buffer.begin() + offset + txSize);
          core::state::Transaction tx;
          tx.deserialize(txData);
          tempTxList.push_back(tx);
          offset += txSize;
        }

        // [STEP 2] Parallel Verification Phase (OpenMP)
        // Verify signatures in parallel to speed up processing.
        // We use a separate integer vector to store validity status (1=Valid,
        // 0=Invalid)
        std::vector<int> validity(numTx, 0);
        int              validCount   = 0;
        int              invalidCount = 0;
#pragma omp parallel for reduction(+ : validCount, invalidCount)
        for (int i = 0; i < numTx; ++i) {
          if (tempTxList[i].verify()) {
            validity[i] = 1;
            validCount++;
          } else {
            validity[i] = 0;
            invalidCount++;
          }
        }

        // [STEP 3] Mempool Population (Serial)
        // Add only valid transactions to the mempool based on the results.
        for (int i = 0; i < numTx; ++i) {
          if (validity[i]) {
            mempool.push_back(tempTxList[i]);
          } else {
            util::Logger::getLogger().error(
                "Shard " + std::to_string(id) +
                ": FAILED validation for transaction " + tempTxList[i].id);
          }
        }

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader: Ingestion complete. Mempool size: " +
            std::to_string(mempool.size()) +
            " (Valid: " + std::to_string(validCount) +
            ", Invalid: " + std::to_string(invalidCount) + ")");
      }

      // 2. CONSENSUS PHASE
      // Instantiate PBFT engine.
      // Intra-shard leader is always Rank 0 of 'communicator'.
      consensus::PBFT pbft(communicator, my_shard_rank, 0, shard_size);

      // Run consensus. Only the leader passes the mempool; replicas pass empty
      // vectors (PBFT handles sync)
      std::string previousBlockHash =
          "0000000000000000000000000000000000000000000000000000000000000000";

      util::Logger::getLogger().info("Shard " + std::to_string(id) +
                                     ": Starting PBFT consensus.");

      util::Timer consensusTimer;
      consensusTimer.start();

      // Pass previousBlockHash to run()
      auto result = pbft.run(mempool, previousBlockHash, runID);
      core::blocks::MicroBlock microBlock        = result.first;
      int                      messagesExchanged = result.second;

      consensusTimer.stop();
      double consensusTime = consensusTimer.elapsedSeconds();

      microBlock.shardId = id;  // Ensure block is tagged with our ID

      std::string role = (my_shard_rank == 0) ? "Leader" : "Member";
      util::ConsensusMetrics::record("total_simulation", runID, id, role, 1,
                                     consensusTime, messagesExchanged, 0);

      util::Logger::getLogger().info(
          "Shard " + std::to_string(id) +
          ": Consensus reached. MicroBlock Hash: " + microBlock.getHash());

      // 3. REPORTING PHASE
      // If I am the Shard Leader, send the valid MicroBlock to the Final
      // Committee Leader
      if (my_shard_rank == 0) {
        std::vector<char> serializedBlock = microBlock.serialize();

        // leaderRank member variable holds the FC Leader's Global Rank
        network::send(serializedBlock, leaderRank, 0, MPI_COMM_WORLD);

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader sent MicroBlock to Final Committee (Rank " +
            std::to_string(leaderRank) + ").");
      }

      return microBlock;
    }

    /**
     * @brief Returns the unique identifier of this shard.
     * @return The shard ID.
     */
    int Shard::getId() const
    {
      return id;
    }

  }  // namespace network
}  // namespace sbmpi
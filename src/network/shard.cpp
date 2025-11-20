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

namespace sbmpi
{
  namespace network
  {

    // leaderRank here refers to the Global Rank of the Final Committee Leader
    // (for sending results)
    Shard::Shard(int id, MPI_Comm comm, int leaderRank)
        : id(id), communicator(comm), leaderRank(leaderRank)
    {
    }

    Shard::~Shard() {}

    void Shard::addTransaction(const core::state::Transaction& tx)
    {
      mempool.push_back(tx);
    }

    MPI_Comm Shard::getCommunicator() const
    {
      return communicator;
    }

    core::blocks::MicroBlock Shard::runConsensus()
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

        for (int i = 0; i < numTx; ++i) {
          int                      txSize = util::unpack_int(buffer, offset);
          std::vector<char>        txData(buffer.begin() + offset,
                                          buffer.begin() + offset + txSize);
          core::state::Transaction tx;
          tx.deserialize(txData);

          // [LOG] Deep validation log
          if (tx.verify()) {
            // util::Logger::getLogger().debug("Shard " + std::to_string(id) +
            // ": Validated transaction " + tx.id);
            mempool.push_back(tx);
          } else {
            util::Logger::getLogger().error(
                "Shard " + std::to_string(id) +
                ": FAILED validation for transaction " + tx.id);
          }

          offset += txSize;
        }

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader: Ingestion complete. Mempool size: " +
            std::to_string(mempool.size()));
      }

      // 2. CONSENSUS PHASE
      // Instantiate PBFT engine.
      // Intra-shard leader is always Rank 0 of 'communicator'.
      consensus::PBFT pbft(communicator, my_shard_rank, 0, shard_size);

      // Run consensus. Only the leader passes the mempool; replicas pass empty
      // vectors (PBFT handles sync)
      util::Logger::getLogger().info("Shard " + std::to_string(id) +
                                     ": Starting PBFT consensus.");
      core::blocks::MicroBlock microBlock = pbft.run(mempool);
      microBlock.shardId = id;  // Ensure block is tagged with our ID

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

    int Shard::getId() const
    {
      return id;
    }

  }  // namespace network
}  // namespace sbmpi
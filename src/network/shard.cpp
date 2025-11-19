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
        for (int i = 0; i < numTx; ++i) {
          int                      txSize = util::unpack_int(buffer, offset);
          std::vector<char>        txData(buffer.begin() + offset,
                                          buffer.begin() + offset + txSize);
          core::state::Transaction tx;
          tx.deserialize(txData);
          mempool.push_back(tx);
          offset += txSize;
        }

        util::Logger::getLogger().debug(
            "Shard " + std::to_string(id) + " Leader received " +
            std::to_string(mempool.size()) + " transactions from Root.");
      }

      // 2. CONSENSUS PHASE
      // Instantiate PBFT engine.
      // Intra-shard leader is always Rank 0 of 'communicator'.
      consensus::PBFT pbft(communicator, my_shard_rank, 0, shard_size);

      // Run consensus. Only the leader passes the mempool; replicas pass empty
      // vectors (PBFT handles sync)
      core::blocks::MicroBlock microBlock = pbft.run(mempool);
      microBlock.shardId = id;  // Ensure block is tagged with our ID

      // 3. REPORTING PHASE
      // If I am the Shard Leader, send the valid MicroBlock to the Final
      // Committee Leader
      if (my_shard_rank == 0) {
        std::vector<char> serializedBlock = microBlock.serialize();

        // leaderRank member variable holds the FC Leader's Global Rank
        network::send(serializedBlock, leaderRank, 0, MPI_COMM_WORLD);

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader sent MicroBlock to Final Committee.");
      }

      return microBlock;
    }

    int Shard::getId() const
    {
      return id;
    }

  }  // namespace network
}  // namespace sbmpi
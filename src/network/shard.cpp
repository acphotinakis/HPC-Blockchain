#include "../../include/sbmpi/network/shard.h"

#include <vector>

#include "../../include/sbmpi/core/blocks/micro_block.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    Shard::Shard(int id, MPI_Comm comm, int leaderRank)
        : id(id), communicator(comm), leaderRank(leaderRank)
    {
    }

    Shard::~Shard() {}

    void Shard::addTransaction(const core::state::Transaction& tx)
    {
      mempool.push_back(tx);
    }

    core::blocks::MicroBlock Shard::runConsensus()
    {
      // This is a placeholder. In a real implementation, this would involve
      // running a consensus algorithm (e.g., PBFT) with transactions from the
      // mempool.
      return core::blocks::MicroBlock(id);
    }

    int Shard::getId() const
    {
      return id;
    }

  }  // namespace network
}  // namespace sbmpi
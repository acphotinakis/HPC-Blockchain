#ifndef SBMPI_SHARD_H
#define SBMPI_SHARD_H

#include <vector>
#include "../core/blocks/micro_block.h"
#include "../core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    class Shard
    {
     public:
      Shard(int id, MPI_Comm comm, int leaderRank);
      ~Shard();
      void addTransaction(const core::state::Transaction& tx);
      MPI_Comm getCommunicator() const;
      core::blocks::MicroBlock runConsensus(int runID);
      int                      getId() const;

     private:
      int                                   id;
      MPI_Comm                              communicator;
      int                                   leaderRank;
      std::vector<core::state::Transaction> mempool;
    };

  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_SHARD_H

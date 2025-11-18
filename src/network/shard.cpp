
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
      core::blocks::MicroBlock runConsensus() {}
      int                      getId() const {}

     private:
      int                                   id;
      MPI_Comm                              communicator;
      int                                   leaderRank;
      std::vector<core::state::Transaction> mempool;
    };

  }  // namespace network
}  // namespace sbmpi
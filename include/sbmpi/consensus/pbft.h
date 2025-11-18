#ifndef SBMPI_PBFT_H
#define SBMPI_PBFT_H

#include <vector>
#include "../core/blocks/micro_block.h"
#include "../core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace consensus
  {

    enum class PBFTMessageType {
      PRE_PREPARE,
      PREPARE,
      COMMIT
    };

    struct PBFTMessage {
      PBFTMessageType type;
      int             senderId;
      std::string     blockHash;
    };

    class PBFT
    {
     public:
      PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes);
      core::blocks::MicroBlock run(
          const std::vector<core::state::Transaction>& transactions);

     private:
      MPI_Comm communicator;
      int      myRank;
      int      leaderRank;
      int      numNodes;
      int      maxFaultyNodes;

      void prePrepare(const core::blocks::MicroBlock& block);
      void prepare();
      void commit();
      void broadcastMessage(const PBFTMessage& msg);
    };

  }  // namespace consensus
}  // namespace sbmpi

#endif  // SBMPI_PBFT_H

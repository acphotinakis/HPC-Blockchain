#ifndef SBMPI_FINAL_COMMITTEE_H
#define SBMPI_FINAL_COMMITTEE_H

#include <vector>
#include "../../core/blocks/macro_block.h"
#include "../../core/blocks/micro_block.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      class FinalCommittee
      {
       public:
        FinalCommittee(MPI_Comm comm);
        ~FinalCommittee();
        std::vector<core::blocks::MicroBlock> collectMicroBlocks(int numShards);
        core::blocks::MacroBlock              assembleMacroBlock(
                         const std::vector<core::blocks::MicroBlock>& microBlocks);

       private:
        MPI_Comm communicator;
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_FINAL_COMMITTEE_H

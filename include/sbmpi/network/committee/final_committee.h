#ifndef SBMPI_FINAL_COMMITTEE_H
#define SBMPI_FINAL_COMMITTEE_H

#include <vector>
#include "../../core/blocks/macro_block.h"
#include "../../core/blocks/micro_block.h"
#include "committee.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Manages the Final Committee responsible for block aggregation.
       * * This class handles the logic for receiving validated MicroBlocks from
       * Shard Leaders and aggregating them into a final MacroBlock for the
       * global blockchain.
       */
      class FinalCommittee : public Committee
      {
       public:
        /**
         * @brief Constructor.
         * @param comm The MPI communicator for the final committee.
         * @param numShards The total number of shards in the system.
         */
        FinalCommittee(MPI_Comm comm,
                       int      numShards = 0);  // Default arg for flexibility

        ~FinalCommittee();

        /**
         * @brief Collects MicroBlocks from all Shard Leaders.
         * * This method blocks until it has received one MicroBlock from every
         * specified shard leader rank.
         * * @param shardLeaderRanks A vector containing the global ranks of all
         * Shard Leaders.
         * @return A vector of the collected, deserialized MicroBlock objects.
         */
        std::vector<core::blocks::MicroBlock> collectMicroBlocks(
            const std::vector<int>& shardLeaderRanks);

        /**
         * @brief Assembles a MacroBlock from a list of MicroBlocks.
         * * Creates a new MacroBlock containing the hashes of the provided
         * MicroBlocks and aggregates their transactions.
         * * @param microBlocks The vector of MicroBlocks to include.
         * * @param prevBlock The previous block in the chain, necessary for linking a new block.
         * @return The newly created MacroBlock.
         */
        core::blocks::MacroBlock assembleMacroBlock(
            const std::vector<core::blocks::MicroBlock>& microBlocks,
            const core::blocks::Block* prevBlock);

       private:
        int num_shards;
        // Removed 'MPI_Comm communicator' to avoid shadowing the base class
        // member
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_FINAL_COMMITTEE_H
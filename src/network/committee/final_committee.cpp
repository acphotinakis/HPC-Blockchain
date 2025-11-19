#include "../../../include/sbmpi/network/committee/final_committee.h"

#include <iostream>
#include <vector>

#include "../../../include/sbmpi/core/blocks/macro_block.h"
#include "../../../include/sbmpi/core/blocks/micro_block.h"
#include "../../../include/sbmpi/network/mpi_wrapper.h"
#include "../../../include/sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      // FIX: Explicitly initialize the Base Class (Committee)
      // We initialize Base with 0s, then populate them correctly in the body
      // using MPI calls.
      FinalCommittee::FinalCommittee(MPI_Comm comm, int num_shards)
          : Committee(comm, 0, 0), num_shards(num_shards)
      {
        // Populate the protected members inherited from Committee
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
      }

      FinalCommittee::~FinalCommittee() {}

      // FIX: Changed signature to accept specific ranks of shard leaders
      std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
          const std::vector<int>& shardLeaderRanks)
      {
        std::vector<core::blocks::MicroBlock> microBlocks;
        int                                   world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        // FIX: Iterate over the specific leader ranks, not generic indices
        for (int leaderRank : shardLeaderRanks) {
          // Receive from the specific Shard Leader
          std::vector<char> microBlockData =
              network::recv(leaderRank, 0, MPI_COMM_WORLD);

          core::blocks::MicroBlock microBlock;
          microBlock.deserialize(microBlockData);
          microBlocks.push_back(microBlock);
        }

        return microBlocks;
      }

      core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
          const std::vector<core::blocks::MicroBlock>& microBlocks)
      {
        core::blocks::MacroBlock macroBlock;

        // In a real system, fetch this from the Blockchain state
        macroBlock.header = core::blocks::BlockHeader(
            0, "prev_hash_placeholder", "merkle_root_placeholder");

        for (const auto& microBlock : microBlocks) {
          macroBlock.addMicroBlock(microBlock);

          // Flatten transactions for global state update
          macroBlock.transactions.insert(macroBlock.transactions.end(),
                                         microBlock.transactions.begin(),
                                         microBlock.transactions.end());
        }
        return macroBlock;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

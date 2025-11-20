#include "../../../include/sbmpi/network/committee/final_committee.h"

#include <iostream>
#include <vector>

#include "../../../include/sbmpi/core/blocks/macro_block.h"
#include "../../../include/sbmpi/core/blocks/micro_block.h"
#include "../../../include/sbmpi/network/mpi_wrapper.h"
#include "../../../include/sbmpi/util/logging.h"
#include "../../../include/sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      //  Explicitly initialize the Base Class (Committee)
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

      //  Changed signature to accept specific ranks of shard leaders
      std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
          const std::vector<int>& shardLeaderRanks)
      {
        std::vector<core::blocks::MicroBlock> microBlocks;
        int                                   world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        util::Logger::getLogger().info(
            "Final Committee: Waiting for MicroBlocks from " +
            std::to_string(shardLeaderRanks.size()) + " shards.");

        //  Iterate over the specific leader ranks, not generic indices
        for (int leaderRank : shardLeaderRanks) {
          util::Logger::getLogger().debug(
              "Final Committee: Waiting for Shard Leader at Rank " +
              std::to_string(leaderRank));
          // Receive from the specific Shard Leader
          std::vector<char> microBlockData =
              network::recv(leaderRank, 0, MPI_COMM_WORLD);

          core::blocks::MicroBlock microBlock;
          microBlock.deserialize(microBlockData);
          microBlocks.push_back(microBlock);

          util::Logger::getLogger().info(
              "Final Committee: Received MicroBlock from Shard Leader Rank " +
              std::to_string(leaderRank) +
              ". Block Hash: " + microBlock.getHash() +
              ". Tx Count: " + std::to_string(microBlock.transactions.size()));
        }

        return microBlocks;
      }

      core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
          const std::vector<core::blocks::MicroBlock>& microBlocks)
      {
        util::Logger::getLogger().info(
            "Final Committee: Assembling MacroBlock from " +
            std::to_string(microBlocks.size()) + " MicroBlocks.");

        core::blocks::MacroBlock macroBlock;

        // In a real system, fetch this from the Blockchain state
        macroBlock.header = core::blocks::BlockHeader(
            0, "prev_hash_placeholder", "merkle_root_placeholder");

        int totalTx = 0;
        for (const auto& microBlock : microBlocks) {
          macroBlock.addMicroBlock(microBlock);

          // Flatten transactions for global state update
          macroBlock.transactions.insert(macroBlock.transactions.end(),
                                         microBlock.transactions.begin(),
                                         microBlock.transactions.end());
          totalTx += microBlock.transactions.size();
        }

        util::Logger::getLogger().info(
            "Final Committee: MacroBlock Assembled. Total Transactions "
            "Finalized: " +
            std::to_string(totalTx));
        return macroBlock;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

/**
 * @file final_committee.cpp
 * @brief Implements the FinalCommittee class responsible for aggregating
 * microblocks into macroblocks.
 */
#include "../../../include/sbmpi/network/committee/final_committee.h"

#include <iostream>
#include <vector>

#include "../../../include/sbmpi/core/blocks/macro_block.h"
#include "../../../include/sbmpi/core/blocks/micro_block.h"
#include "../../../include/sbmpi/network/mpi_wrapper.h"
#include "../../../include/sbmpi/util/logging.h"
#include "../../../include/sbmpi/util/serialization.h"
#include "mpi.h"

#include "../../../include/sbmpi/util/metrics.h"
#include "../../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Represents the Final Committee, responsible for collecting
       * microblocks from shard leaders and assembling them into macroblocks.
       *
       * Inherits from the base Committee class.
       */
      FinalCommittee::FinalCommittee(MPI_Comm comm, int num_shards)
          : Committee(comm, 0, 0), num_shards(num_shards)
      {
        // Populate the protected members inherited from Committee
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
      }

      /**
       * @brief Destructor for FinalCommittee.
       */
      FinalCommittee::~FinalCommittee() {}

      /**
       * @brief Collects microblocks from all shard leaders.
       *
       * The Final Committee members listen for microblocks sent by the
       * designated shard leaders.
       * @param shardLeaderRanks A vector of global MPI ranks of the shard
       * leaders.
       * @return A vector of collected MicroBlock objects.
       */
      std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
          const std::vector<int>& shardLeaderRanks)
      {
        std::vector<core::blocks::MicroBlock> microBlocks;
        int                                   world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        util::Logger::getLogger().info(
            "Final Committee: Rank " + std::to_string(world_rank) +
            " waiting for MicroBlocks from " +
            std::to_string(shardLeaderRanks.size()) + " shards.");

        // Iterate over the specific leader ranks, not generic indices
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

        util::Logger::getLogger().info(
            "Final Committee: Received " + std::to_string(microBlocks.size()) +
            " MicroBlocks from " + std::to_string(shardLeaderRanks.size()) +
            " shards.");

        return microBlocks;
      }

      /**
       * @brief Assembles a MacroBlock from a collection of MicroBlocks.
       *
       * This process involves adding the microblock hashes to the macroblock
       * and flattening all transactions from the microblocks into the
       * macroblock's transaction list.
       * @param microBlocks A vector of MicroBlock objects collected from
       * shards.
       * @return A newly assembled MacroBlock.
       */
      core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
          const std::vector<core::blocks::MicroBlock>& microBlocks,
          const core::blocks::Block* prevBlock, int runID)
      {
        util::Timer blockCreationTimer;
        blockCreationTimer.start();

        util::Logger::getLogger().info(
            "Final Committee: Assembling MacroBlock from " +
            std::to_string(microBlocks.size()) + " MicroBlocks.");

        core::blocks::MacroBlock macroBlock;

        // In a real system, fetch this from the Blockchain state
        macroBlock.header = core::blocks::BlockHeader(
            prevBlock->header.height + 1, prevBlock->getHash(),
            prevBlock->header.merkleRoot);

        int totalTx = 0;
        for (const auto& microBlock : microBlocks) {
          macroBlock.addMicroBlock(microBlock);

          // Flatten transactions for global state update
          macroBlock.transactions.insert(macroBlock.transactions.end(),
                                         microBlock.transactions.begin(),
                                         microBlock.transactions.end());
          totalTx += microBlock.transactions.size();
        }

        blockCreationTimer.stop();
        double blockCreationTime = blockCreationTimer.elapsedSeconds();
        util::BlockMetrics::record("total_simulation", runID,
                                   macroBlock.getHash(), "Macro", totalTx,
                                   blockCreationTime, prevBlock->getHash());

        util::Logger::getLogger().info(
            "Final Committee: MacroBlock Assembled. Total Transactions "
            "Finalized: " +
            std::to_string(totalTx));
        return macroBlock;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

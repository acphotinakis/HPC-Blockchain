#ifndef SBMPI_FINAL_COMMITTEE_H
#define SBMPI_FINAL_COMMITTEE_H

#include <vector>
#include "../core/blocks/macro_block.h"
#include "../core/blocks/micro_block.h"
#include "mpi.h"

/**
 * @file final_committee.h
 * @brief Defines the FinalCommittee class for aggregating blocks from shards.
 *
 * The FinalCommittee is responsible for collecting validated MicroBlocks from
 * all shards, verifying them, and assembling them into a MacroBlock. This
 * MacroBlock represents the final, authoritative state of the blockchain for a
 * given epoch.
 */

class FinalCommittee
{
 public:
  /**
   * @brief Constructor for the FinalCommittee.
   *
   * @param comm The MPI communicator for the final committee members.
   */
  FinalCommittee(MPI_Comm comm);

  /**
   * @brief Destructor for the FinalCommittee.
   *
   * Cleans up MPI resources.
   */
  ~FinalCommittee();

  /**
   * @brief Listens for and collects MicroBlocks from all shard leaders.
   *
   * This function will likely involve MPI_Gather or repeated MPI_Recv calls
   * to receive the results of each shard's consensus.
   *
   * @param numShards The total number of shards to expect blocks from.
   * @return A vector of the collected MicroBlocks.
   */
  std::vector<MicroBlock> collectMicroBlocks(int numShards);

  /**
   * @brief Assembles a MacroBlock from a collection of MicroBlocks.
   *
   * This involves creating a MacroBlock, adding the hashes of the microblocks,
   * and finalizing the block for inclusion in the global blockchain.
   *
   * @param microBlocks The vector of MicroBlocks to assemble.
   * @return The newly created MacroBlock.
   */
  MacroBlock assembleMacroBlock(const std::vector<MicroBlock>& microBlocks);

 private:
  MPI_Comm communicator;
  // Internal state for the final committee would be managed here.
};

#endif  // SBMPI_FINAL_COMMITTEE_H
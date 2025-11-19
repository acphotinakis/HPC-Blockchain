#include "sbmpi/network/committee/final_committee.h"

#include <vector>

#include "sbmpi/core/blocks/macro_block.h"
#include "sbmpi/core/blocks/micro_block.h"
#include "sbmpi/network/mpi_wrapper.h"
#include "sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi {
namespace network {
namespace committee {

FinalCommittee::FinalCommittee(MPI_Comm comm) : communicator(comm) {}

FinalCommittee::~FinalCommittee() {}

std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
    int numShards) {
  std::vector<core::blocks::MicroBlock> microBlocks;
  int                                   world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  for (int i = 0; i < numShards; ++i) {
    // Assuming each shard leader sends its microblock to the final committee
    // This is a simplified model. In a real system, there would be a more
    // complex routing and verification process.
    std::vector<char> microBlockData = network::recv(i, 0, communicator);
    core::blocks::MicroBlock microBlock;
    microBlock.deserialize(microBlockData);
    microBlocks.push_back(microBlock);
  }
  return microBlocks;
}

core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
    const std::vector<core::blocks::MicroBlock>& microBlocks) {
  core::blocks::MacroBlock macroBlock;
  // In a real system, the previous hash would come from the blockchain.
  // The merkle root would be calculated from the micro-block hashes and
  // transactions.
  macroBlock.header =
      core::blocks::BlockHeader(0, "prev_hash_placeholder", "merkle_root_placeholder");

  for (const auto& microBlock : microBlocks) {
    macroBlock.addMicroBlock(microBlock);
    // Also add transactions from microblocks to macroblock (simplified)
    macroBlock.transactions.insert(macroBlock.transactions.end(),
                                   microBlock.transactions.begin(),
                                   microBlock.transactions.end());
  }
  return macroBlock;
}

}  // namespace committee
}  // namespace network
}  // namespace sbmpi

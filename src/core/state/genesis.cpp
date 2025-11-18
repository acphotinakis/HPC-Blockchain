#include "../../include/sbmpi/core/state/genesis.h"

#include <memory>

namespace sbmpi {
namespace core {
namespace state {

std::unique_ptr<blocks::MacroBlock> createGenesisBlock() {
  auto genesisBlock = std::make_unique<blocks::MacroBlock>();
  genesisBlock->header.height = 0;
  genesisBlock->header.previousHash = "0";
  // In a real scenario, merkle root would be calculated from genesis
  // transactions
  genesisBlock->header.merkleRoot = "0";
  genesisBlock->header.timestamp = std::chrono::system_clock::now();
  return genesisBlock;
}

}  // namespace state
}  // namespace core
}  // namespace sbmpi

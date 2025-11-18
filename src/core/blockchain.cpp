#include "../../include/sbmpi/core/blockchain.h"

#include <memory>
#include <vector>

#include "../../include/sbmpi/core/state/genesis.h"

namespace sbmpi {
namespace core {

Blockchain::Blockchain() { createGenesisBlock(); }

void Blockchain::createGenesisBlock() {
  chain.push_back(state::createGenesisBlock());
}

void Blockchain::addBlock(std::unique_ptr<blocks::Block> block) {
  if (block) {
    // Basic validation
    const blocks::Block* latest = getLatestBlock();
    if (latest && latest->getHash() == block->header.previousHash &&
        latest->header.height + 1 == block->header.height) {
      chain.push_back(std::move(block));
    }
  }
}

const blocks::Block* Blockchain::getBlock(int height) const {
  if (height >= 0 && height < chain.size()) {
    return chain[height].get();
  }
  return nullptr;
}

const blocks::Block* Blockchain::getLatestBlock() const {
  if (chain.empty()) {
    return nullptr;
  }
  return chain.back().get();
}

bool Blockchain::validate() const {
  if (chain.size() <= 1) {
    return true;
  }
  for (size_t i = 1; i < chain.size(); ++i) {
    const auto& current = chain[i];
    const auto& previous = chain[i - 1];
    if (current->header.previousHash != previous->getHash()) {
      return false;
    }
    if (current->header.height != previous->header.height + 1) {
      return false;
    }
  }
  return true;
}

int Blockchain::getHeight() const {
  return chain.empty() ? -1 : static_cast<int>(chain.size()) - 1;
}

}  // namespace core
}  // namespace sbmpi
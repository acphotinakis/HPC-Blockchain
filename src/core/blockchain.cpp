#include "../../include/sbmpi/core/blockchain.h"
#include <iostream>

namespace sbmpi
{
  namespace core
  {

    Blockchain::Blockchain()
    {
      // The chain starts with the Genesis Block
      m_chain_.push_back(createGenesisBlock());
    }

    bool Blockchain::addBlock(const Block& new_block)
    {
      // Basic validation: new block must point to the previous block's hash
      if (new_block.getHeader().previous_hash != getLastBlock().getHash()) {
        std::cerr << "Validation failed: New block's previous_hash does not "
                     "match last block's hash."
                  << std::endl;
        return false;
      }

      // A more robust validation would also check the new block's own hash
      // and its internal integrity, but we keep it simple here.

      m_chain_.push_back(new_block);
      return true;
    }

    bool Blockchain::validateChain() const
    {
      for (size_t i = 1; i < m_chain_.size(); ++i) {
        const Block& current_block  = m_chain_[i];
        const Block& previous_block = m_chain_[i - 1];

        // 1. Check if the block's stored hash is correct
        if (current_block.getHash() != current_block.calculateHash()) {
          return false;
        }

        // 2. Check if it points to the correct previous block
        if (current_block.getHeader().previous_hash !=
            previous_block.getHash()) {
          return false;
        }
      }
      return true;
    }

    const Block& Blockchain::getLastBlock() const
    {
      // m_chain_ is never empty because of the Genesis Block
      return m_chain_.back();
    }

    size_t Blockchain::getHeight() const
    {
      return m_chain_.size();
    }

    Block Blockchain::createGenesisBlock() const
    {
      // The first block in the chain, with arbitrary valid data
      return Block(0, "0", -1); // Block num 0, no previous hash, no shard
    }

  }  // namespace core
}  // namespace sbmpi

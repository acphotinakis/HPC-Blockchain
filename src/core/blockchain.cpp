/**
 * @file blockchain.cpp
 * @brief Implements the Blockchain class for managing a chain of blocks.
 */
#include "../../include/sbmpi/core/blockchain.h"
#include "../../include/sbmpi/util/logging.h"

#include <memory>
#include <vector>

#include "../../include/sbmpi/core/state/genesis.h"

namespace sbmpi
{
  namespace core
  {

    /**
     * @brief Represents a blockchain, managing a sequence of blocks.
     *
     * This class provides functionality to add blocks, retrieve blocks by height,
     * get the latest block, and validate the integrity of the chain.
     */
    Blockchain::Blockchain()
    {
      createGenesisBlock();
    }

    /**
     * @brief Creates and adds the genesis block to the blockchain.
     *
     * This is typically the first block in the chain, with predefined properties.
     */
    void Blockchain::createGenesisBlock()
    {
      chain.push_back(state::createGenesisBlock());
    }

    /**
     * @brief Adds a new block to the blockchain after basic validation.
     *
     * The block is added only if its previous hash matches the latest block's hash
     * and its height is one greater than the latest block's height.
     * @param block A unique pointer to the block to be added.
     */
    void Blockchain::addBlock(std::unique_ptr<blocks::Block> block)
    {
      if (block) {
        // Basic validation
        const blocks::Block* latest = getLatestBlock();
        if (latest && latest->getHash() == block->header.previousHash &&
            latest->header.height + 1 == block->header.height) {
          chain.push_back(std::move(block));
        } else {
          util::Logger::getLogger().error("Could not add block to blockchain!");
        }
      }
    }

    /**
     * @brief Retrieves a block from the blockchain by its height.
     * @param height The height (index) of the block to retrieve.
     * @return A pointer to the Block if found, nullptr otherwise.
     */
    const blocks::Block* Blockchain::getBlock(int height) const
    {
      if (height >= 0 && height < chain.size()) {
        return chain[height].get();
      }
      return nullptr;
    }

    /**
     * @brief Retrieves the collection of blocks in the blockchain.
     * @return A vector containing a collection of blocks in the blockchain.
    */
    const std::vector<std::unique_ptr<blocks::Block>>& Blockchain::getBlockchain() const 
    {
      return chain;
    }

    /**
     * @brief Retrieves the latest block in the blockchain.
     * @return A pointer to the latest Block if the chain is not empty, nullptr otherwise.
     */
    const blocks::Block* Blockchain::getLatestBlock() const
    {
      if (chain.empty()) {
        return nullptr;
      }
      return chain.back().get();
    }

    /**
     * @brief Validates the integrity of the blockchain.
     *
     * Checks if each block's previous hash matches the hash of the preceding block
     * and if block heights are sequential.
     * @return True if the blockchain is valid, false otherwise.
     */
    bool Blockchain::validate() const
    {
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

    /**
     * @brief Returns the current height of the blockchain.
     * @return The height of the latest block (0-indexed), or -1 if the chain is empty.
     */
    int Blockchain::getHeight() const
    {
      return chain.empty() ? -1 : static_cast<int>(chain.size()) - 1;
    }

  } // namespace core
} // namespace sbmpi
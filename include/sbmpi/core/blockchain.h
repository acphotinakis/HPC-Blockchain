#ifndef SBMPI_BLOCKCHAIN_H
#define SBMPI_BLOCKCHAIN_H

#include <memory>
#include <vector>
#include "blocks/block.h"

/**
 * @file blockchain.h
 * @brief Defines the Blockchain class, which manages the chain of blocks.
 *
 * The Blockchain class is responsible for maintaining the distributed ledger.
 * It holds a sequence of blocks, provides methods for adding new blocks,
 * and can validate the integrity of the entire chain. This implementation
 * stores blocks as unique pointers to support polymorphism (Micro/MacroBlocks).
 */

class Blockchain
{
 public:
  /**
   * @brief Default constructor. Initializes the blockchain, possibly with a
   * genesis block.
   */
  Blockchain();

  /**
   * @brief Adds a new block to the end of the chain.
   *
   * The block is validated before being added.
   *
   * @param block A unique_ptr to the block to be added.
   */
  void addBlock(std::unique_ptr<Block> block);

  /**
   * @brief Gets the block at a specific height.
   *
   * @param height The height of the block to retrieve.
   * @return A const pointer to the block, or nullptr if not found.
   */
  const Block* getBlock(int height) const;

  /**
   * @brief Gets the most recently added block.
   *
   * @return A const pointer to the latest block, or nullptr if the chain is
   * empty.
   */
  const Block* getLatestBlock() const;

  /**
   * @brief Validates the integrity of the entire blockchain.
   *
   * Checks hash linkages and other rules for all blocks in the chain.
   *
   * @return true if the chain is valid, false otherwise.
   */
  bool validate() const;

  /**
   * @brief Returns the current height of the blockchain.
   *
   * @return The number of blocks in the chain.
   */
  int getHeight() const;

 private:
  // The chain of blocks, using unique_ptr to handle polymorphic block types.
  std::vector<std::unique_ptr<Block>> chain;

  /**
   * @brief Creates the very first block in the chain (Genesis Block).
   */
  void createGenesisBlock();
};

#endif  // SBMPI_BLOCKCHAIN_H
/**
 * @file blockchain.h
 * @brief Defines the Blockchain class, which manages the chain of blocks.
 *
 * @headerfile blockchain.h
 *
 * @details
 * The corresponding blockchain.cpp will implement the chain management and
 * validation logic.
 *
 * This class represents the final, global, ordered blockchain. In this
 * project, it will be exclusively owned and managed by the
 * `FinalCommittee` module. The Final Committee's
 * primary role is to receive validated micro-blocks from all shards
 * and use the `addBlock()` method to append them to this global chain.
 *
 * Each individual Shard might maintain a small, local "shard-chain," but
 * this class represents the canonical, aggregated ledger.
 */

#pragma once

#include "block.h"
#include <vector>

namespace sbmpi {
namespace core {

/**
 * @class Blockchain
 * @brief Manages a list of blocks, forming the distributed ledger.
 */
class Blockchain {
public:
  /**
   * @brief Constructor. Initializes the chain with a Genesis block.
   */
  Blockchain();

  /**
   * @brief Adds a new block to the chain after validation.
   *
   * @param new_block The block to add, received from a shard leader
   * or created by the final committee.
   * @return true if the block was added successfully, false otherwise.
   */
  bool addBlock(const Block &new_block);

  /**
   * @brief Validates the integrity of the entire blockchain.
   *
   * Checks all hash pointers to ensure the chain has not been
   * tampered with.
   *
   * @return true if the chain is valid, false otherwise.
   */
  bool validateChain() const;

  /**
   * @brief Gets the last block in the chain.
   * @return A constant reference to the last block.
   */
  const Block &getLastBlock() const;

  /**
   * @brief Gets the current height (number of blocks) of the chain.
   * @return The size of the chain.
   */
  size_t getHeight() const;

private:
  /**
   * @brief The chain of blocks, stored as a vector.
   */
  std::vector<Block> m_chain_;

  /**
   * @brief Creates the first block (Genesis Block) in the chain.
   * @return The Genesis Block.
   */
  Block createGenesisBlock() const;
};

} // namespace core
} // namespace sbmpi
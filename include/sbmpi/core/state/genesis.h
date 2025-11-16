#ifndef SBMPI_GENESIS_H
#define SBMPI_GENESIS_H

#include <memory>
#include "../blocks/macro_block.h"

/**
 * @file genesis.h
 * @brief Defines the function for creating the genesis block of the blockchain.
 *
 * This file provides the interface for the `createGenesisBlock` function, which
 * is implemented in `src/core/state/genesis.cpp`. The genesis block is the
 * first block in the chain and is created without a preceding block.
 */

namespace sbmpi
{
  namespace genesis
  {

    /**
     * @brief Creates the genesis block for the blockchain.
     *
     * The genesis block is a special MacroBlock that serves as the foundation
     * of the entire chain.
     *
     * @return A unique_ptr to the newly created genesis block.
     */
    std::unique_ptr<MacroBlock> createGenesisBlock();

  }  // namespace genesis
}  // namespace sbmpi

#endif  // SBMPI_GENESIS_H
/**
 * @file genesis.cpp
 * @brief Implements the creation of the genesis block.
 */
#include "../../../include/sbmpi/core/state/genesis.h"
#include <memory>
#include "../../../include/sbmpi/core/blocks/macro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Creates a unique pointer to a new genesis MacroBlock.
       *
       * The genesis block is the first block in the blockchain, initialized with
       * a height of 0, a "0" previous hash, a "0" Merkle root (as there are no
       * initial transactions to hash), and the current timestamp.
       * @return A unique_ptr to the newly created MacroBlock representing the genesis block.
       */
      std::unique_ptr<sbmpi::core::blocks::MacroBlock> createGenesisBlock()
      {
        auto genesisBlock = std::make_unique<sbmpi::core::blocks::MacroBlock>();
        genesisBlock->header.height = 0;
        genesisBlock->header.previousHash = "0";
        // In a real scenario, merkle root would be calculated from genesis
        // transactions
        genesisBlock->header.merkleRoot = "0";
        genesisBlock->header.timestamp = std::chrono::system_clock::now();
        return genesisBlock;
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

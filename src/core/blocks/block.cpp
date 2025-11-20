/**
 * @file block.cpp
 * @brief Implements the base Block class.
 */
#include "../../../include/sbmpi/core/blocks/block.h"
#include <string>
#include <vector>
#include "../../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Retrieves the cryptographic hash of the block.
       *
       * This method delegates the hashing responsibility to the block's header.
       * @return A std::string representing the SHA256 hash of the block header.
       */
      std::string Block::getHash() const
      {
        return header.hash();
      }

    } // namespace blocks
  } // namespace core

} // namespace sbmpi
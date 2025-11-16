#ifndef SBMPI_MACRO_BLOCK_H
#define SBMPI_MACRO_BLOCK_H

#include <vector>
#include "block.h"
#include "micro_block.h"

/**
 * @file macro_block.h
 * @brief Defines the MacroBlock class, a block that aggregates multiple
 * MicroBlocks.
 *
 * A MacroBlock is created by the final committee. It does not contain
 * transactions directly, but rather a list of hashes of the MicroBlocks that
 * were validated and accepted from the shards during a given epoch. This forms
 * the final, authoritative chain.
 */

class MacroBlock : public Block
{
 public:
  // A list of hashes of the MicroBlocks included in this MacroBlock.
  std::vector<std::string> microBlockHashes;

  /**
   * @brief Default constructor for MacroBlock.
   */
  MacroBlock();

  /**
   * @brief Gets the type of the block.
   *
   * @return The string "MacroBlock".
   */
  std::string getType() const override;

  /**
   * @brief Adds a MicroBlock to the MacroBlock by its hash.
   *
   * @param microBlock The MicroBlock to include.
   */
  void addMicroBlock(const MicroBlock& microBlock);

  /**
   * @brief Serializes the MacroBlock into a byte vector.
   *
   * @return A std::vector<char> containing the serialized block data.
   */
  std::vector<char> serialize() const override;

  /**
   * @brief Deserializes a byte vector into a MacroBlock object.
   *
   * @param data The byte vector to deserialize.
   */
  void deserialize(const std::vector<char>& data) override;
};

#endif  // SBMPI_MACRO_BLOCK_H
#ifndef SBMPI_MACRO_BLOCK_H
#define SBMPI_MACRO_BLOCK_H

#include <vector>
#include "block.h"
#include "micro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class MacroBlock : public Block
      {
       public:
        std::vector<std::string> microBlockHashes;

        MacroBlock();
        std::string       getType() const override;
        void              addMicroBlock(const MicroBlock& microBlock);
        std::vector<char> serialize() const override;
        void              deserialize(const std::vector<char>& data) override;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MACRO_BLOCK_H

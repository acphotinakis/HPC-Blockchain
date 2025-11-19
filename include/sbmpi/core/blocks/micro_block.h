#ifndef SBMPI_MICRO_BLOCK_H
#define SBMPI_MICRO_BLOCK_H

#include "block.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class MicroBlock : public Block
      {
       public:
        int shardId;

        MicroBlock();
        MicroBlock(int shardId);
        std::string       getType() const override;
        std::vector<char> serialize() const override;
        void              deserialize(const std::vector<char>& data) override;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MICRO_BLOCK_H

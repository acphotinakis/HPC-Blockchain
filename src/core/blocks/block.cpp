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

      std::string Block::getHash() const
      {
        return header.hash();
      }

    }  // namespace blocks
  }  // namespace core

}  // namespace sbmpi
#ifndef SBMPI_GENESIS_H
#define SBMPI_GENESIS_H

#include <memory>
#include "../blocks/macro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      std::unique_ptr<blocks::MacroBlock> createGenesisBlock();

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_GENESIS_H

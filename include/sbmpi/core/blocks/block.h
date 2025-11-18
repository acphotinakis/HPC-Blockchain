#ifndef SBMPI_BLOCK_H
#define SBMPI_BLOCK_H

#include <string>
#include <vector>
#include "../state/transaction.h"
#include "blockheader.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class Block
      {
       public:
        BlockHeader                     header;
        std::vector<state::Transaction> transactions;

        virtual ~Block() = default;
        std::string               getHash() const;
        virtual std::string       getType() const               = 0;
        virtual std::vector<char> serialize() const             = 0;
        virtual void deserialize(const std::vector<char>& data) = 0;

       protected:
        Block() = default;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCK_H

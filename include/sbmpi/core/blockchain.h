#ifndef SBMPI_BLOCKCHAIN_H
#define SBMPI_BLOCKCHAIN_H

#include <memory>
#include <vector>
#include "blocks/block.h"

namespace sbmpi
{
  namespace core
  {

    class Blockchain
    {
     public:
      Blockchain();
      void                 addBlock(std::unique_ptr<blocks::Block> block);
      const blocks::Block* getBlock(int height) const;
      const blocks::Block* getLatestBlock() const;
      bool                 validate() const;
      int                  getHeight() const;

     private:
      std::vector<std::unique_ptr<blocks::Block>> chain;
      void                                        createGenesisBlock();
    };

  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCKCHAIN_H

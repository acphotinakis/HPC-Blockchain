#ifndef SBMPI_BLOCKHEADER_H
#define SBMPI_BLOCKHEADER_H

#include <chrono>
#include <string>
#include <vector>

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class BlockHeader
      {
       public:
        int                                   height;
        std::string                           previousHash;
        std::string                           merkleRoot;
        std::chrono::system_clock::time_point timestamp;

        BlockHeader();
        BlockHeader(int height, const std::string& previousHash,
                    const std::string& merkleRoot);
        std::string       hash() const;
        std::vector<char> serialize() const;
        void              deserialize(const std::vector<char>& data);
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCKHEADER_H
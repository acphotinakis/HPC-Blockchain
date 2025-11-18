#include "../../include/sbmpi/core/blocks/micro_block.h"

#include <vector>

#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi {
namespace core {
namespace blocks {

MicroBlock::MicroBlock() : shardId(0) {}

MicroBlock::MicroBlock(int shardId) : shardId(shardId) {}

std::string MicroBlock::getType() const { return "MicroBlock"; }

std::vector<char> MicroBlock::serialize() const {
  std::vector<char> buffer;

  std::vector<char> headerData = header.serialize();
  util::pack(static_cast<int>(headerData.size()), buffer);
  buffer.insert(buffer.end(), headerData.begin(), headerData.end());

  util::pack(shardId, buffer);

  util::pack(static_cast<int>(transactions.size()), buffer);
  for (const auto& tx : transactions) {
    std::vector<char> txData = tx.serialize();
    util::pack(static_cast<int>(txData.size()), buffer);
    buffer.insert(buffer.end(), txData.begin(), txData.end());
  }

  return buffer;
}

void MicroBlock::deserialize(const std::vector<char>& data) {
  int offset = 0;

  int headerSize = util::unpack_int(data, offset);
  std::vector<char> headerVec(data.begin() + offset,
                              data.begin() + offset + headerSize);
  header.deserialize(headerVec);
  offset += headerSize;

  shardId = util::unpack_int(data, offset);

  transactions.clear();
  int numTransactions = util::unpack_int(data, offset);
  for (int i = 0; i < numTransactions; ++i) {
    int txSize = util::unpack_int(data, offset);
    std::vector<char> txData(data.begin() + offset,
                             data.begin() + offset + txSize);
    state::Transaction tx;
    tx.deserialize(txData);
    transactions.push_back(tx);
    offset += txSize;
  }
}

}  // namespace blocks
}  // namespace core
}  // namespace sbmpi

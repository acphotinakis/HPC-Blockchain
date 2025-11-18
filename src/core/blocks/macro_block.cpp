#include "../../include/sbmpi/core/blocks/macro_block.h"

#include <vector>

#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi {
namespace core {
namespace blocks {

MacroBlock::MacroBlock() {}

std::string MacroBlock::getType() const { return "MacroBlock"; }

void MacroBlock::addMicroBlock(const MicroBlock& microBlock) {
  microBlockHashes.push_back(microBlock.getHash());
}

std::vector<char> MacroBlock::serialize() const {
  std::vector<char> buffer;

  // Serialize header
  std::vector<char> headerData = header.serialize();
  util::pack(static_cast<int>(headerData.size()), buffer);
  buffer.insert(buffer.end(), headerData.begin(), headerData.end());

  // Serialize micro block hashes
  util::pack(static_cast<int>(microBlockHashes.size()), buffer);
  for (const auto& hash : microBlockHashes) {
    util::pack(hash, buffer);
  }

  // Macro blocks can also contain transactions (e.g. rewards, cross-shard
  // settlements)
  util::pack(static_cast<int>(transactions.size()), buffer);
  for (const auto& tx : transactions) {
    std::vector<char> txData = tx.serialize();
    util::pack(static_cast<int>(txData.size()), buffer);
    buffer.insert(buffer.end(), txData.begin(), txData.end());
  }

  return buffer;
}

void MacroBlock::deserialize(const std::vector<char>& data) {
  int offset = 0;

  // Deserialize header
  int headerSize = util::unpack_int(data, offset);
  std::vector<char> headerVec(data.begin() + offset,
                              data.begin() + offset + headerSize);
  header.deserialize(headerVec);
  offset += headerSize;

  // Deserialize micro block hashes
  microBlockHashes.clear();
  int numHashes = util::unpack_int(data, offset);
  for (int i = 0; i < numHashes; ++i) {
    microBlockHashes.push_back(util::unpack_string(data, offset));
  }

  // Deserialize transactions
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

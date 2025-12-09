/**
 * @file macro_block.cpp
 * @brief Implements the MacroBlock class, which aggregates MicroBlocks.
 */
#include "../../../include/sbmpi/core/blocks/macro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Represents a MacroBlock, which is a block that aggregates
       * multiple MicroBlocks.
       *
       * MacroBlocks are typically used in sharded blockchain architectures to
       * finalize the state of multiple shards. They contain references (hashes)
       * to the MicroBlocks they include, and can also contain their own
       * transactions (e.g., cross-shard transactions, rewards).
       */
      MacroBlock::MacroBlock() {}

      /**
       * @brief Returns the type name of the block.
       * @return A std::string indicating the block type, "MacroBlock".
       */
      std::string MacroBlock::getType() const
      {
        return "MacroBlock";
      }

      /**
       * @brief Adds the hash of a MicroBlock to this MacroBlock.
       * @param microBlock The MicroBlock whose hash is to be added.
       */
      void MacroBlock::addMicroBlock(const MicroBlock& microBlock)
      {
        microBlockHashes.push_back(microBlock.getHash());
      }

      /**
       * @brief Serializes the MacroBlock into a vector of characters.
       *
       * The serialization includes the block header, the hashes of all
       * contained MicroBlocks, and any transactions directly within this
       * MacroBlock.
       * @return A std::vector<char> containing the serialized MacroBlock data.
       */
      std::vector<char> MacroBlock::serialize() const
      {
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

      /**
       * @brief Deserializes a vector of characters into a MacroBlock object.
       *
       * Reconstructs the MacroBlock from its serialized byte representation,
       * including its header, micro block hashes, and transactions.
       * @param data The std::vector<char> containing the serialized MacroBlock
       * data.
       */
      void MacroBlock::deserialize(const std::vector<char>& data)
      {
        int offset = 0;

        // Deserialize header
        int               headerSize = util::unpack_int(data, offset);
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
          int                txSize = util::unpack_int(data, offset);
          std::vector<char>  txData(data.begin() + offset,
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

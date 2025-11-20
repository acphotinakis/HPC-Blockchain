/**
 * @file micro_block.cpp
 * @brief Implements the MicroBlock class, representing a block within a single shard.
 */
#include "../../../include/sbmpi/core/blocks/micro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Represents a MicroBlock, a block specific to a single shard.
       *
       * MicroBlocks contain transactions processed within a particular shard
       * and are eventually aggregated into MacroBlocks by the Final Committee.
       */
      MicroBlock::MicroBlock() : shardId(0) {}

      /**
       * @brief Constructs a MicroBlock with a specified shard ID.
       * @param shardId The identifier of the shard this MicroBlock belongs to.
       */
      MicroBlock::MicroBlock(int shardId) : shardId(shardId) {}

      /**
       * @brief Returns the type name of the block.
       * @return A std::string indicating the block type, "MicroBlock".
       */
      std::string MicroBlock::getType() const
      {
        return "MicroBlock";
      }

      /**
       * @brief Serializes the MicroBlock into a vector of characters.
       *
       * The serialization includes the block header, the shard ID, and all
       * transactions contained within this MicroBlock.
       * @return A std::vector<char> containing the serialized MicroBlock data.
       */
      std::vector<char> MicroBlock::serialize() const
      {
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

      /**
       * @brief Deserializes a vector of characters into a MicroBlock object.
       *
       * Reconstructs the MicroBlock from its serialized byte representation,
       * including its header, shard ID, and transactions.
       * @param data The std::vector<char> containing the serialized MicroBlock data.
       */
      void MicroBlock::deserialize(const std::vector<char>& data)
      {
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

    } // namespace blocks
  } // namespace core
} // namespace sbmpi

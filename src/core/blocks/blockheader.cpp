/**
 * @file blockheader.cpp
 * @brief Implements the BlockHeader class for blockchain blocks.
 */
#include "../../../include/sbmpi/core/blocks/blockheader.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../../../include/sbmpi/util/crypto.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {
      /**
       * @brief Default constructor for BlockHeader.
       * Initializes height to 0, previousHash and merkleRoot to empty strings,
       * and timestamp to the current system time.
       */
      BlockHeader::BlockHeader()
          : height(0),
            previousHash(""),
            merkleRoot(""),
            timestamp(std::chrono::system_clock::now())
      {
      }

      /**
       * @brief Main constructor for BlockHeader.
       * @param height_ The height of the block in the blockchain.
       * @param previousHash_ The hash of the previous block.
       * @param merkleRoot_ The Merkle root of all transactions in the block.
       */
      BlockHeader::BlockHeader(int height_, const std::string& previousHash_,
                               const std::string& merkleRoot_)
          : height(height_),
            previousHash(previousHash_),
            merkleRoot(merkleRoot_),
            timestamp(std::chrono::system_clock::now())
      {
      }

      /**
       * @brief Calculates the Keccak-256 cryptographic hash of the block
       * header.
       *
       * The hash is computed by concatenating the block's height, previous
       * hash, Merkle root, and timestamp (in milliseconds) into a single string
       * and then applying Keccak-256.
       * @return A std::string representing the Keccak-256 hash of the block
       * header.
       */
      std::string BlockHeader::hash() const
      {
        // Convert timestamp → integer (portable)
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                      timestamp.time_since_epoch())
                      .count();

        // Concatenate input (canonical serialization for hashing)
        std::ostringstream input;
        input << height << '|' << previousHash << '|' << merkleRoot << '|'
              << ts;

        // Convert our data into a vector of bytes
        const std::string                data = input.str();
        const std::vector<unsigned char> dataBytes(data.begin(), data.end());
        // Return the hex string of the hash
        return util::toHex(util::keccak256(dataBytes));
      }

      /**
       * @brief Serializes the BlockHeader into a vector of characters.
       *
       * The format is:
       *   [height:int32]
       *   [timestamp:int64 ms]
       *   [prevHashLen:int32][prevHash bytes]
       *   [merkleLen:int32][merkle bytes]
       * @return A std::vector<char> containing the serialized block header
       * data.
       */
      std::vector<char> BlockHeader::serialize() const
      {
        std::vector<char> buffer;

        auto appendInt32 = [&](int32_t v) {
          char b[4];
          std::memcpy(b, &v, 4);
          buffer.insert(buffer.end(), b, b + 4);
        };

        auto appendInt64 = [&](int64_t v) {
          char b[8];
          std::memcpy(b, &v, 8);
          buffer.insert(buffer.end(), b, b + 8);
        };

        auto appendString = [&](const std::string& s) {
          appendInt32(static_cast<int32_t>(s.size()));
          buffer.insert(buffer.end(), s.begin(), s.end());
        };

        // Serializable timestamp
        int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();

        appendInt32(height);
        appendInt64(ts);
        appendString(previousHash);
        appendString(merkleRoot);

        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a BlockHeader object.
       *
       * Reconstructs the BlockHeader from its serialized byte representation.
       * Throws std::runtime_error if the buffer is too small or data is
       * malformed.
       * @param data The std::vector<char> containing the serialized block
       * header data.
       */
      void BlockHeader::deserialize(const std::vector<char>& data)
      {
        if (data.size() < 4 + 8) {
          throw std::runtime_error(
              "BlockHeader::deserialize: buffer too small");
        }

        std::size_t offset = 0;

        auto readInt32 = [&](int32_t& out) {
          if (offset + 4 > data.size())
            throw std::runtime_error("BlockHeader::deserialize: out of range");
          std::memcpy(&out, &data[offset], 4);
          offset += 4;
        };

        auto readInt64 = [&](int64_t& out) {
          if (offset + 8 > data.size())
            throw std::runtime_error("BlockHeader::deserialize: out of range");
          std::memcpy(&out, &data[offset], 8);
          offset += 8;
        };

        auto readString = [&](std::string& s) {
          int32_t len = 0;
          readInt32(len);
          if (len < 0 || offset + len > data.size())
            throw std::runtime_error(
                "BlockHeader::deserialize: invalid string length");

          s.assign(&data[offset], len);
          offset += len;
        };

        int64_t tsMillis = 0;

        readInt32(height);
        readInt64(tsMillis);
        readString(previousHash);
        readString(merkleRoot);

        timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(tsMillis));
      }

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi
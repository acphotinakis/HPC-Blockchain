#include "../../include/sbmpi/core/blocks/blockheader.h"
#include <openssl/sha.h>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sbmpi {
namespace core {
namespace blocks {

// -------------------------------------------------------------
// Utility: convert bytes to hex string
// -------------------------------------------------------------
static std::string toHex(const unsigned char* data, std::size_t len)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < len; ++i)
    oss << std::setw(2) << static_cast<int>(data[i]);

    return oss.str();
}

// -------------------------------------------------------------
// Default constructor
// -------------------------------------------------------------
BlockHeader::BlockHeader()
    : height(0),
      previousHash(""),
      merkleRoot(""),
      timestamp(std::chrono::system_clock::now())
{
}

// -------------------------------------------------------------
// Main constructor
// -------------------------------------------------------------
BlockHeader::BlockHeader(int height_, const std::string& previousHash_,
                         const std::string& merkleRoot_)
    : height(height_),
      previousHash(previousHash_),
      merkleRoot(merkleRoot_),
      timestamp(std::chrono::system_clock::now())
{
}

// -------------------------------------------------------------
// Cryptographic Hash (SHA-256)
// -------------------------------------------------------------
std::string BlockHeader::hash() const
{
    // Convert timestamp → integer (portable)
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch())
                .count();

    // Concatenate input (canonical serialization for hashing)
    std::ostringstream input;
    input << height << '|' << previousHash << '|' << merkleRoot << '|' << ts;

    const std::string data = input.str();

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
           digest);

    return toHex(digest, SHA256_DIGEST_LENGTH);
}

// -------------------------------------------------------------
// Serialization
// Format:
//   [height:int32]
//   [timestamp:int64 ms]
//   [prevHashLen:int32][prevHash bytes]
//   [merkleLen:int32][merkle bytes]
// -------------------------------------------------------------
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

// -------------------------------------------------------------
// Deserialization
// -------------------------------------------------------------
void BlockHeader::deserialize(const std::vector<char>& data)
{
    if (data.size() < 4 + 8) {
    throw std::runtime_error("BlockHeader::deserialize: buffer too small");
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
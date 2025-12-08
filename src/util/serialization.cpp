/**
 * @file serialization.cpp
 * @brief Provides utility functions for serializing and deserializing primitive types and strings into a byte buffer.
 *
 * These functions are essential for converting structured data into a format
 * suitable for network transmission or storage, and for reconstructing it back.
 */
#include "../../include/sbmpi/util/serialization.h"
#include <cstring>
#include <stdexcept>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Packs an integer value into a character vector buffer.
     * @param value The integer to pack.
     * @param buffer The std::vector<char> buffer to append the packed integer to.
     */
    void pack(int value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int));
    }

    void pack_int64(int64_t value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int64_t));
    }

    void pack_uint64(uint64_t value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(uint64_t));
    }

    /**
     * @brief Packs a double-precision floating-point value into a character vector buffer.
     * @param value The double to pack.
     * @param buffer The std::vector<char> buffer to append the packed double to.
     */
    void pack(double value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(double));
    }

    /**
     * @brief Packs a string value into a character vector buffer.
     *
     * The string's length is packed first as an integer, followed by the string's characters.
     * @param value The string to pack.
     * @param buffer The std::vector<char> buffer to append the packed string to.
     */
    void pack(const std::string& value, std::vector<char>& buffer)
    {
      int len = value.length();
      pack(len, buffer);
      buffer.insert(buffer.end(), value.begin(), value.end());
    }

    void pack(const std::vector<unsigned char>& value, std::vector<char>& buffer)
    {
        int len = value.size();
        pack(len, buffer);
        buffer.insert(buffer.end(), value.begin(), value.end());
    }

    /**
     * @brief Unpacks an integer value from a character vector buffer.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked integer value.
     */
    int unpack_int(const std::vector<char>& buffer, int& offset)
    {
      int value;
      std::memcpy(&value, buffer.data() + offset, sizeof(int));
      offset += sizeof(int);
      return value;
    }

    int64_t unpack_int64_t(const std::vector<char>& buffer, int& offset)
    {
        int64_t value;
        if (offset + sizeof(int64_t) > buffer.size()) {
            throw std::runtime_error("Not enough data to unpack int64_t");
        }
        
        std::memcpy(&value, &buffer[offset], sizeof(int64_t));
        offset += sizeof(int64_t);
        return value;
    }

    uint64_t unpack_uint64_t(const std::vector<char>& buffer, int& offset)
    {
        uint64_t value;
        if (offset + sizeof(uint64_t) > buffer.size()) {
            throw std::runtime_error("Not enough data to unpack uint64_t");
        }
        
        std::memcpy(&value, &buffer[offset], sizeof(uint64_t));
        offset += sizeof(uint64_t);
        return value;
    }

    /**
     * @brief Unpacks a double-precision floating-point value from a character vector buffer.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked double value.
     */
    double unpack_double(const std::vector<char>& buffer, int& offset)
    {
      double value;
      std::memcpy(&value, buffer.data() + offset, sizeof(double));
      offset += sizeof(double);
      return value;
    }

    /**
     * @brief Unpacks a string value from a character vector buffer.
     *
     * Reads the string's length first, then extracts the characters.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked string value.
     */
    std::string unpack_string(const std::vector<char>& buffer, int& offset)
    {
      int len = unpack_int(buffer, offset);
      std::string value(buffer.data() + offset, len);
      offset += len;
      return value;
    }

    std::vector<unsigned char> unpack_vector_unsigned_char(const std::vector<char>& buffer, int& offset)
    {
        int len = unpack_int(buffer, offset);
        std::vector<unsigned char> result;
        result.reserve(len);
        
        for (int i = 0; i < len; ++i) {
            result.push_back(static_cast<unsigned char>(buffer[offset++]));
        }
        return result;
    }

  } // namespace util
} // namespace sbmpi

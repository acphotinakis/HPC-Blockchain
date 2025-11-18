#include "../../include/sbmpi/util/serialization.h"
#include <cstring>

namespace sbmpi
{
  namespace util
  {

    void pack(int value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int));
    }

    void pack(double value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(double));
    }

    void pack(const std::string& value, std::vector<char>& buffer)
    {
      int len = value.length();
      pack(len, buffer);
      buffer.insert(buffer.end(), value.begin(), value.end());
    }

    int unpack_int(const std::vector<char>& buffer, int& offset)
    {
      int value;
      std::memcpy(&value, buffer.data() + offset, sizeof(int));
      offset += sizeof(int);
      return value;
    }

    double unpack_double(const std::vector<char>& buffer, int& offset)
    {
      double value;
      std::memcpy(&value, buffer.data() + offset, sizeof(double));
      offset += sizeof(double);
      return value;
    }

    std::string unpack_string(const std::vector<char>& buffer, int& offset)
    {
      int         len = unpack_int(buffer, offset);
      std::string value(buffer.data() + offset, len);
      offset += len;
      return value;
    }

  }  // namespace util
}  // namespace sbmpi

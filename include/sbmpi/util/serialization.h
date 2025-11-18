#ifndef SBMPI_SERIALIZATION_H
#define SBMPI_SERIALIZATION_H

#include <string>
#include <vector>

namespace sbmpi {
namespace util {

void pack(int value, std::vector<char>& buffer);
void pack(double value, std::vector<char>& buffer);
void pack(const std::string& value, std::vector<char>& buffer);

int unpack_int(const std::vector<char>& buffer, int& offset);
double unpack_double(const std::vector<char>& buffer, int& offset);
std::string unpack_string(const std::vector<char>& buffer, int& offset);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_SERIALIZATION_H

#ifndef SBMPI_SERIALIZATION_H
#define SBMPI_SERIALIZATION_H

#include <string>
#include <vector>

/**
 * @file serialization.h
 * @brief Provides helper functions for serializing and deserializing data.
 *
 * This file declares a set of utility functions for converting various data
 * types (like int, double, string) into a byte vector (`std::vector<char>`) and
 * back. This is essential for packing complex data structures into a format
 * that can be transmitted over MPI. The implementations are in
 * `src/util/serialization.cpp`.
 */

namespace sbmpi
{
  namespace serialization
  {

    // Pack functions (serialize)
    void pack(int value, std::vector<char>& buffer);
    void pack(double value, std::vector<char>& buffer);
    void pack(const std::string& value, std::vector<char>& buffer);

    // Unpack functions (deserialize)
    int         unpack_int(const std::vector<char>& buffer, int& offset);
    double      unpack_double(const std::vector<char>& buffer, int& offset);
    std::string unpack_string(const std::vector<char>& buffer, int& offset);

  }  // namespace serialization
}  // namespace sbmpi

#endif  // SBMPI_SERIALIZATION_H
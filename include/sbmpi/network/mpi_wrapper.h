#ifndef SBMPI_MPI_WRAPPER_H
#define SBMPI_MPI_WRAPPER_H

#include <vector>
#include "mpi.h"

/**
 * @file mpi_wrapper.h
 * @brief Provides a higher-level wrapper around common MPI communication
 * patterns.
 *
 * This file declares a set of functions that simplify MPI operations, such as
 * sending and receiving data that is stored in `std::vector<char>`. This
 * abstracts away some of the boilerplate MPI code. The implementations are in
 * `src/network/mpi_wrapper.cpp`.
 */

namespace sbmpi
{
  namespace mpi_wrapper
  {

    /**
     * @brief Sends a vector of data to a destination process.
     *
     * @param data The vector of characters to send.
     * @param dest The rank of the destination process.
     * @param tag The message tag.
     * @param comm The MPI communicator to use.
     */
    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm);

    /**
     * @brief Receives a vector of data from a source process.
     *
     * @param source The rank of the source process.
     * @param tag The message tag.
     * @param comm The MPI communicator to use.
     * @return A std::vector<char> containing the received data.
     */
    std::vector<char> recv(int source, int tag, MPI_Comm comm);

    /**
     * @brief Broadcasts a vector of data from a root process to all others.
     *
     * @param data The vector of characters to broadcast (input for root, output
     * for others).
     * @param root The rank of the root process.
     * @param comm The MPI communicator to use.
     */
    void bcast(std::vector<char>& data, int root, MPI_Comm comm);

  }  // namespace mpi_wrapper
}  // namespace sbmpi

#endif  // SBMPI_MPI_WRAPPER_H
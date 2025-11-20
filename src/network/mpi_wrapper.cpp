/**
 * @file mpi_wrapper.cpp
 * @brief Provides a simplified wrapper around common MPI communication functions.
 *
 * These functions facilitate sending, receiving, and broadcasting serialized
 * data (as std::vector<char>) between MPI processes.
 */
#include "../../include/sbmpi/network/mpi_wrapper.h"

#include <vector>

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    /**
     * @brief Sends a vector of characters to a specified destination MPI process.
     * @param data The std::vector<char> containing the data to send.
     * @param dest The rank of the destination MPI process.
     * @param tag The message tag.
     * @param comm The MPI communicator to use for sending.
     */
    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm)
    {
      MPI_Send(data.data(), static_cast<int>(data.size()), MPI_CHAR, dest, tag,
               comm);
    }

    /**
     * @brief Receives a vector of characters from a specified source MPI process.
     *
     * Uses MPI_Probe to determine the incoming message size before allocating
     * a buffer and receiving the data.
     * @param source The rank of the source MPI process (or MPI_ANY_SOURCE).
     * @param tag The message tag (or MPI_ANY_TAG).
     * @param comm The MPI communicator to use for receiving.
     * @return A std::vector<char> containing the received data.
     */
    std::vector<char> recv(int source, int tag, MPI_Comm comm)
    {
      MPI_Status status;
      MPI_Probe(source, tag, comm, &status);

      int size;
      MPI_Get_count(&status, MPI_CHAR, &size);

      std::vector<char> buffer(size);
      MPI_Recv(buffer.data(), size, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG,
               comm, MPI_STATUS_IGNORE);

      return buffer;
    }

    /**
     * @brief Broadcasts a vector of characters from a root MPI process to all
     *        other processes in the communicator.
     *
     * The root process sends the size of the data, then the data itself.
     * Non-root processes receive the size, resize their buffer, and then receive the data.
     * @param data A reference to the std::vector<char> to be broadcast (input for root, output for others).
     * @param root The rank of the root MPI process.
     * @param comm The MPI communicator to use for broadcasting.
     */
    void bcast(std::vector<char>& data, int root, MPI_Comm comm)
    {
      int rank;
      MPI_Comm_rank(comm, &rank);

      int size = static_cast<int>(data.size());
      MPI_Bcast(&size, 1, MPI_INT, root, comm);

      if (rank != root) {
        data.resize(size);
      }

      MPI_Bcast(data.data(), size, MPI_CHAR, root, comm);
    }

  } // namespace network
} // namespace sbmpi

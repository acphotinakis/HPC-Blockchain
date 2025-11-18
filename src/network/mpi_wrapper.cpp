#include "../../include/sbmpi/network/mpi_wrapper.h"

#include <vector>

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm)
    {
      MPI_Send(data.data(), static_cast<int>(data.size()), MPI_CHAR, dest, tag,
               comm);
    }

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

  }  // namespace network
}  // namespace sbmpi

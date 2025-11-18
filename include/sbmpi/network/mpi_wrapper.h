#ifndef SBMPI_MPI_WRAPPER_H
#define SBMPI_MPI_WRAPPER_H

#include <vector>
#include "mpi.h"

namespace sbmpi {
namespace network {

void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm);
std::vector<char> recv(int source, int tag, MPI_Comm comm);
void bcast(std::vector<char>& data, int root, MPI_Comm comm);

}  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_MPI_WRAPPER_H

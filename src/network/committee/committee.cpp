#include "../../../include/sbmpi/network/committee/committee.h"

#include <vector>

#include "../../../include/sbmpi/core/node.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      MPI_Comm Committee::getCommunicator() const
      {
        return communicator;
      }

      int Committee::getSize() const
      {
        return size;
      }

      int Committee::getRank() const
      {
        return rank;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi
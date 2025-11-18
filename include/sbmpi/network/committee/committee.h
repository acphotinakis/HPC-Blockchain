#ifndef SBMPI_COMMITTEE_H
#define SBMPI_COMMITTEE_H

#include <vector>
#include "../../core/node.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      class Committee
      {
       public:
        virtual ~Committee() = default;
        MPI_Comm getCommunicator() const;
        int      getSize() const;
        int      getRank() const;

       protected:
        MPI_Comm                communicator;
        std::vector<core::Node> nodes;
        int                     size;
        int                     rank;
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_COMMITTEE_H

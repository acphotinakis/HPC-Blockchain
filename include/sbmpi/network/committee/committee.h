#ifndef SBMPI_COMMITTEE_H
#define SBMPI_COMMITTEE_H

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Base class representing a group of nodes (a committee) in the
       * network.
       * * Provides basic functionality common to all committees, such as
       * accessing the MPI communicator and committee metadata.
       */
      class Committee
      {
       public:
        /**
         * @brief Constructor.
         * @param comm The MPI communicator for this committee.
         * @param size The number of nodes in this committee.
         * @param rank The rank of this node within the committee.
         */
        Committee(MPI_Comm comm, int size, int rank)
            : communicator(comm), size(size), rank(rank)
        {
        }

        virtual ~Committee() = default;

        /**
         * @brief Get the MPI Communicator for this committee.
         * @return The MPI_Comm object.
         */
        MPI_Comm getCommunicator() const;

        /**
         * @brief Get the size of the committee (number of nodes).
         * @return The size as an integer.
         */
        int getSize() const;

        /**
         * @brief Get the rank of the current node within this committee.
         * @return The rank as an integer.
         */
        int getRank() const;

       protected:
        MPI_Comm communicator;
        int      size;
        int      rank;
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_COMMITTEE_H
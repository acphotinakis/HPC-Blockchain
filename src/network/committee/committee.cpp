/**
 * @file committee.cpp
 * @brief Implements the base Committee class for managing MPI communicators within committees.
 */
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

      /**
       * @brief Base class for managing a group of nodes (a committee) using MPI.
       *
       * Provides common functionality for committees, such as accessing their
       * MPI communicator, size, and the rank of the current process within that communicator.
       */
      // Constructor is implicitly defined in header, no need to document here.

      /**
       * @brief Returns the MPI communicator associated with this committee.
       * @return The MPI_Comm object for the committee.
       */
      MPI_Comm Committee::getCommunicator() const
      {
        return communicator;
      }

      /**
       * @brief Returns the total number of processes in this committee.
       * @return The size of the committee.
       */
      int Committee::getSize() const
      {
        return size;
      }

      /**
       * @brief Returns the rank of the current process within this committee's communicator.
       * @return The local rank within the committee.
       */
      int Committee::getRank() const
      {
        return rank;
      }

    } // namespace committee
  } // namespace network
} // namespace sbmpi
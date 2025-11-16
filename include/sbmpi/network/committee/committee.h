#ifndef SBMPI_COMMITTEE_H
#define SBMPI_COMMITTEE_H

#include <vector>
#include "../../core/node.h"
#include "mpi.h"

/**
 * @file committee.h
 * @brief Defines an abstract base class for different types of committees.
 *
 * This file provides the interface for the Committee class, which serves as a
 * base for concrete committee implementations like Shard and FinalCommittee.
 * It defines the common properties of a committee, such as its MPI communicator
 * and the list of nodes it contains. The implementation is in
 * `src/network/committee/committee.cpp`.
 */

class Committee
{
 public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~Committee() = default;

  /**
   * @brief Gets the MPI communicator for this committee.
   * @return The MPI_Comm handle.
   */
  MPI_Comm getCommunicator() const;

  /**
   * @brief Gets the number of nodes in the committee.
   * @return The size of the committee.
   */
  int getSize() const;

  /**
   * @brief Gets the rank of the current process within the committee.
   * @return The committee-local rank.
   */
  int getRank() const;

 protected:
  MPI_Comm          communicator;
  std::vector<Node> nodes;
  int               size;
  int               rank;
};

#endif  // SBMPI_COMMITTEE_H
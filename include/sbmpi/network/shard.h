#ifndef SBMPI_SHARD_H
#define SBMPI_SHARD_H

#include <vector>
#include "../core/blocks/micro_block.h"
#include "../core/transaction.h"
#include "mpi.h"

/**
 * @file shard.h
 * @brief Defines the Shard class, which manages a shard committee and its
 * operations.
 *
 * The Shard class encapsulates the logic for a single shard. It holds the
 * shard-specific MPI communicator, manages the transaction pool for the shard,
 * and orchestrates the PBFT consensus process to produce a MicroBlock.
 */

class Shard
{
 public:
  /**
   * @brief Constructor for a Shard.
   *
   * @param id The unique ID of this shard.
   * @param comm The MPI communicator for the nodes in this shard.
   * @param leaderRank The rank of the leader node within the shard
   * communicator.
   */
  Shard(int id, MPI_Comm comm, int leaderRank);

  /**
   * @brief Destructor for a Shard.
   *
   * Cleans up MPI resources, such as the communicator.
   */
  ~Shard();

  /**
   * @brief Adds a transaction to the shard's local memory pool.
   *
   * @param tx The transaction to add.
   */
  void addTransaction(const Transaction& tx);

  /**
   * @brief Runs the PBFT consensus algorithm on the current transaction pool.
   *
   * This is the core function of the shard, resulting in the creation of a
   * signed MicroBlock if consensus is successful.
   *
   * @return A MicroBlock containing the validated transactions.
   */
  MicroBlock runConsensus();

  /**
   * @brief Gets the shard's ID.
   * @return The unique identifier for the shard.
   */
  int getId() const;

 private:
  int                      id;
  MPI_Comm                 communicator;
  int                      leaderRank;
  std::vector<Transaction> mempool;
  // Internal state for the PBFT process would be managed here or in a dedicated
  // PBFT class.
};

#endif  // SBMPI_SHARD_H
#ifndef SBMPI_MEMPOOL_H
#define SBMPI_MEMPOOL_H

#include <mutex>
#include <vector>
#include "../core/state/transaction.h"

/**
 * @file mempool.h
 * @brief Defines a thread-safe memory pool for pending transactions.
 *
 * This file provides the interface for the Mempool class, which is responsible
 * for storing and managing transactions that have been received but not yet
 * included in a block. The implementation in `src/core/mempool/mempool.cpp`
 * ensures that access to the pool is thread-safe.
 */

class Mempool
{
 public:
  Mempool();

  /**
   * @brief Adds a transaction to the mempool.
   *
   * @param tx The transaction to add.
   * @return true if the transaction was added successfully, false otherwise
   * (e.g., if it already exists).
   */
  bool add(const Transaction& tx);

  /**
   * @brief Removes a transaction from the mempool.
   *
   * @param transactionId The ID of the transaction to remove.
   */
  void remove(const std::string& transactionId);

  /**
   * @brief Gets a batch of transactions from the mempool.
   *
   * @param maxCount The maximum number of transactions to retrieve.
   * @return A vector of transactions.
   */
  std::vector<Transaction> getTransactions(size_t maxCount);

  /**
   * @brief Returns the number of transactions currently in the mempool.
   *
   * @return The size of the mempool.
   */
  size_t size() const;

 private:
  mutable std::mutex       mtx;
  std::vector<Transaction> transactions;
};

#endif  // SBMPI_MEMPOOL_H
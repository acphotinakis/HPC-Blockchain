/**
 * @file mempool.cpp
 * @brief Implements the Mempool class for managing pending transactions.
 */
#include "sbmpi/core/mempool/mempool.h"
#include <algorithm>
#include <vector>
#include "sbmpi/core/state/transaction.h"
namespace sbmpi
{
  namespace core
  {
    namespace mempool
    {

      /**
       * @brief Manages a pool of unconfirmed transactions.
       *
       * The Mempool is responsible for storing transactions that have been received
       * but not yet included in a block. It provides thread-safe operations for
       * adding, removing, and retrieving transactions.
       */
      Mempool::Mempool() {}

      /**
       * @brief Adds a transaction to the mempool.
       *
       * Prevents duplicate transactions based on their ID.
       * @param tx The transaction to add.
       * @return True if the transaction was added, false if it was a duplicate.
       */
      bool Mempool::add(const state::Transaction& tx)
      {
        std::lock_guard<std::mutex> lock(mtx);
        // Prevent duplicates
        auto it = std::find_if(
            transactions.begin(), transactions.end(),
            [&](const state::Transaction& t) { return t.id == tx.id; });
        if (it == transactions.end()) {
          transactions.push_back(tx);
          return true;
        }
        return false;
      }

      /**
       * @brief Removes a transaction from the mempool by its ID.
       * @param transactionId The ID of the transaction to remove.
       */
      void Mempool::remove(const std::string& transactionId)
      {
        std::lock_guard<std::mutex> lock(mtx);
        transactions.erase(
            std::remove_if(transactions.begin(), transactions.end(),
                           [&](const state::Transaction& tx) {
                             return tx.id == transactionId;
                           }),
            transactions.end());
      }

      /**
       * @brief Retrieves a specified number of transactions from the mempool.
       *
       * The retrieved transactions are removed from the mempool.
       * @param maxCount The maximum number of transactions to retrieve.
       * @return A vector of transactions.
       */
      std::vector<state::Transaction> Mempool::getTransactions(size_t maxCount)
      {
        std::lock_guard<std::mutex> lock(mtx);
        size_t count = std::min(maxCount, transactions.size());
        std::vector<state::Transaction> result(transactions.begin(),
                                               transactions.begin() + count);
        transactions.erase(transactions.begin(), transactions.begin() + count);
        return result;
      }

      /**
       * @brief Returns the current number of transactions in the mempool.
       * @return The size of the mempool.
       */
      size_t Mempool::size() const
      {
        std::lock_guard<std::mutex> lock(mtx);
        return transactions.size();
      }

    } // namespace mempool
  } // namespace core
} // namespace sbmpi

#ifndef SBMPI_STATE_H
#define SBMPI_STATE_H

#include <map>
#include <string>
#include "transaction.h"

/**
 * @file state.h
 * @brief Defines the class for managing the blockchain's world state.
 *
 * This file provides the interface for the State class, which is responsible
 * for tracking the current state of all accounts and balances in the
 * blockchain. The implementation in `src/core/state/state.cpp` applies
 * transactions to update the state accordingly.
 */

class State
{
 public:
  State();

  /**
   * @brief Applies a transaction to the current state.
   *
   * This function updates the balances of the sender and receiver accounts.
   *
   * @param tx The transaction to apply.
   * @return true if the transaction was applied successfully, false otherwise.
   */
  bool apply(const Transaction& tx);

  /**
   * @brief Gets the balance of a specific account.
   *
   * @param address The address of the account.
   * @return The balance of the account.
   */
  double getBalance(const std::string& address) const;

 private:
  // A map from account addresses to their balances.
  std::map<std::string, double> balances;
};

#endif  // SBMPI_STATE_H
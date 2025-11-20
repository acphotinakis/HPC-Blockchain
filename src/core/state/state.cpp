/**
 * @file state.cpp
 * @brief Implements the State class for managing the blockchain's global state.
 */
#include "../../../include/sbmpi/core/state/state.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Manages the global state of the blockchain, primarily account balances.
       *
       * This class provides functionality to apply transactions and query account balances.
       * It initializes with a genesis address having a predefined balance.
       */
      State::State()
      {
        // Maybe some initial state, e.g. for genesis
        balances["genesis_address"] = 1000000.0;
      }

      /**
       * @brief Applies a transaction to the current state, updating account balances.
       *
       * Performs basic validation: verifies the transaction's signature and checks
       * if the sender has sufficient funds. If valid, it debits the sender and
       * credits the receiver.
       * @param tx The transaction to apply.
       * @return True if the transaction was successfully applied, false otherwise (e.g., invalid signature, insufficient funds).
       */
      bool State::apply(const Transaction& tx)
      {
        if (!tx.verify()) {
          return false;
        }

        auto from_it = balances.find(tx.from);
        if (from_it == balances.end() || from_it->second < tx.amount) {
          // Sender does not exist or has insufficient funds
          return false;
        }

        from_it->second -= tx.amount;

        auto to_it = balances.find(tx.to);
        if (to_it == balances.end()) {
          balances[tx.to] = tx.amount;
        } else {
          to_it->second += tx.amount;
        }

        return true;
      }

      /**
       * @brief Retrieves the balance of a given address.
       * @param address The address whose balance is to be retrieved.
       * @return The balance of the address, or 0.0 if the address does not exist in the state.
       */
      double State::getBalance(const std::string& address) const
      {
        auto it = balances.find(address);
        if (it != balances.end()) {
          return it->second;
        }
        return 0.0;
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

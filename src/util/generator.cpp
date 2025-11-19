#include "../../include/sbmpi/util/generator.h"
#include <iostream>
#include <random>
#include <string>

namespace sbmpi
{
  namespace util
  {

    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count)
    {
      std::vector<sbmpi::core::state::Transaction> transactions;
      transactions.reserve(count);

      // CRITICAL: Use a fixed seed (42) for deterministic, repeatable results.
      // This ensures consistent benchmarking between serial and parallel runs.
      std::mt19937 gen(42);

      // Simulate a pool of 10,000 unique users
      std::uniform_int_distribution<> userDist(1, 10000);
      // Random transaction amounts between 0.01 and 1000.00
      std::uniform_real_distribution<> amountDist(0.01, 1000.0);

      for (size_t i = 0; i < count; ++i) {
        // Generate Sender
        std::string sender = "user_" + std::to_string(userDist(gen));

        // Generate Receiver (ensure it is different from sender)
        std::string receiver;
        do {
          receiver = "user_" + std::to_string(userDist(gen));
        } while (sender == receiver);

        double amount = amountDist(gen);

        // Instantiate the Transaction
        // Note: The constructor provided in transaction.h handles from, to, and
        // amount.
        sbmpi::core::state::Transaction tx(sender, receiver, amount);

        // Assign a unique, deterministic ID
        tx.id = std::to_string(i);

        // Sign the transaction
        // In a real app, we would look up the user's specific private key.
        // For simulation, we generate a consistent dummy key string based on
        // the sender.
        tx.sign("private_key_" + sender);

        transactions.push_back(tx);
      }

      return transactions;
    }

  }  // namespace util
}  // namespace sbmpi
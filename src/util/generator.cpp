/**
 * @file generator.cpp
 * @brief Implements utility functions for generating mock data, specifically transactions.
 */
#include "../../include/sbmpi/util/generator.h"
#include "../../include/sbmpi/util/logging.h"
#include <algorithm>  // For std::min
#include <iomanip>    // For std::setw, std::fixed, std::setprecision
#include <iostream>
#include <filesystem>
#include <sys/stat.h>
#include <cassert>
#include <fstream>
#include <random>
#include <string>

static const std::string POOLDIR{"pools/"};

namespace sbmpi
{
  namespace util
  {
    /**
     * @brief Generates a specified number of `Wallet` objects.

     * Returns a collection of populated `Wallet` objects, each with their "publicKey",
     * "privateKey", and "address" member variables populated. The values are all random
     * which is standard for most cryptocurrency implementations.
     * @param count Specifies how many `Wallet` instances to instantiate.
     * @return A std::vector containing popluated `Wallet` instances.
     */
    std::vector<sbmpi::core::state::Wallet> generateMockWallets(size_t count) {
      std::vector<sbmpi::core::state::Wallet> wallets;
      wallets.reserve(count);

      for (size_t i = 0; i < count; i++) {
        sbmpi::core::state::Wallet newWallet; // Constructor populates member variables
        wallets.push_back(newWallet);
      }

      return wallets;
    }

    /**
     * @brief Writes a collection of `Wallet` object data to a specified JSON file.
     *
     * The method needs to receive a file name with a .json extension or the method will
     * not output. If the file name is valid, then the method iterates through the collection
     * of `Wallet` objects and creates a collection of JSON objects that contain a: "publicKey",
     * "privateKey", and "address" string.
     * @param filename A correct JSON filename.
     * @param wallets A vector instance containing populated `Wallet` objects.
     */
    void writeWalletAddresses(const std::string& filename, std::vector<sbmpi::core::state::Wallet> wallets) {
      // Check if the filename is large enough to hold .json extension
      if (filename.size() <= 5) {
        sbmpi::util::Logger::getLogger().error("Filename length is incorrect!");
        return;
      }

      // See if the filename includes .json extension
      int fileExtIndex = filename.find_last_of(".");
      std::string fileExtension = filename.substr(fileExtIndex, filename.size());
      if (fileExtension.compare(".json") != 0) {
        sbmpi::util::Logger::getLogger().error("Wallet file extension is not \'.json\'!");
        return;
      }
      
      // Check if directory already exists, then gracefully continue
      mkdir(POOLDIR.c_str(), 0777);

      // Open in truncate mode, overwrite old run wallets
      std::ofstream walletPool(POOLDIR + filename, std::ios::trunc);
      if (walletPool.is_open()) {
        walletPool << "{\n\t\"wallets\": [\n";
        for (size_t i = 0; i < wallets.size(); i++) {
          walletPool << "\t\t{"
                     << "\n\t\t\t\"publicKey\": \"" << wallets[i].publicKeyHex << "\","
                     << "\n\t\t\t\"privateKey\": \"" << wallets[i].privateKeyHex << "\","
                     << "\n\t\t\t\"address\": \"" << wallets[i].address << "\""
                     << "\n\t\t}";
          
          // If last item, then don't create a trailing comma
          if (i + 1 != wallets.size()) {
            walletPool << ",";
          }
          walletPool << "\n";
        }

        walletPool << "\t]\n}";
        walletPool.close();
      }
    }

    /**
     * @brief Generates a specified number of mock transactions.
     *
     * Transactions are generated with random senders, receivers (ensuring different
     * from sender), and amounts. A fixed seed is used for the random number
     * generator to ensure deterministic and repeatable results for benchmarking.
     * Each transaction is assigned a unique, deterministic ID and a simulated signature.
     * The first 10 generated transactions are pretty-printed to stdout.
     *
     * @param count The number of mock transactions to generate.
     * @return A std::vector of generated core::state::Transaction objects.
     */
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
        sbmpi::core::state::Transaction tx(sender, receiver, amount);

        // Assign a unique, deterministic ID
        tx.id = std::to_string(i);

        // Sign the transaction
        // Note: The signature here is simulated for the purpose of the mock
        // data.
        tx.sign("private_key_" + sender);

        transactions.push_back(tx);
      }

      // --- NEW LOGIC: Pretty Print First 10 Transactions ---
      size_t transactionsToPrint = std::min(count, (size_t)10);

      if (transactionsToPrint > 0) {
        std::cout << "\n--- Generated Mock Transactions (First "
                  << transactionsToPrint << " of " << count << ") ---"
                  << std::endl;
        std::cout << "ID       | FROM (User)  | TO (User)    | AMOUNT"
                  << std::endl;
        std::cout << "--------------------------------------------------------"
                  << std::endl;

        // Iterate only through the first N transactions
        for (size_t i = 0; i < transactionsToPrint; ++i) {
          const auto& tx = transactions[i];

          // Output format: ID | FROM | TO | AMOUNT
          std::cout << std::left << std::setw(8) << tx.id << "| " << std::left
                    << std::setw(12) << tx.from << "| " << std::left
                    << std::setw(12) << tx.to << "| $" << std::fixed
                    << std::setprecision(2) << tx.amount << std::endl;
        }
        std::cout
            << "--------------------------------------------------------\n"
            << std::endl;
      }

      return transactions;
    }

  } // namespace util
} // namespace sbmpi
// #include "../../include/sbmpi/util/generator.h"
// #include <iostream>
// #include <random>
// #include <string>

// namespace sbmpi
// {
//   namespace util
//   {

//     std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
//         size_t count)
//     {
//       std::vector<sbmpi::core::state::Transaction> transactions;
//       transactions.reserve(count);

//       // CRITICAL: Use a fixed seed (42) for deterministic, repeatable
//       results.
//       // This ensures consistent benchmarking between serial and parallel
//       runs. std::mt19937 gen(42);

//       // Simulate a pool of 10,000 unique users
//       std::uniform_int_distribution<> userDist(1, 10000);
//       // Random transaction amounts between 0.01 and 1000.00
//       std::uniform_real_distribution<> amountDist(0.01, 1000.0);

//       for (size_t i = 0; i < count; ++i) {
//         // Generate Sender
//         std::string sender = "user_" + std::to_string(userDist(gen));

//         // Generate Receiver (ensure it is different from sender)
//         std::string receiver;
//         do {
//           receiver = "user_" + std::to_string(userDist(gen));
//         } while (sender == receiver);

//         double amount = amountDist(gen);

//         // Instantiate the Transaction
//         // Note: The constructor provided in transaction.h handles from, to,
//         and
//         // amount.
//         sbmpi::core::state::Transaction tx(sender, receiver, amount);

//         // Assign a unique, deterministic ID
//         tx.id = std::to_string(i);

//         // Sign the transaction
//         // In a real app, we would look up the user's specific private key.
//         // For simulation, we generate a consistent dummy key string based on
//         // the sender.
//         tx.sign("private_key_" + sender);

//         transactions.push_back(tx);
//       }

//       return transactions;
//     }

//   }  // namespace util
// }  // namespace sbmpi
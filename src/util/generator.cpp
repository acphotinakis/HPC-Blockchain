/**
 * @file generator.cpp
 * @brief Implements utility functions for generating mock data, specifically
 * transactions.
 */
#include "../../include/sbmpi/util/generator.h"
#include <sys/stat.h>
#include <algorithm>  // For std::min
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>  // For std::setw, std::fixed, std::setprecision
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <unordered_map>
#include "../../include/sbmpi/util/logging.h"

static const std::string POOLDIR{"pools/"};
using json = nlohmann::json;

namespace sbmpi
{
  namespace util
  {
    /**
     * Checks if a file exists, is regular, and has data.
     *
     * @param filename The filename to evaluate.
     * @returns True if the above conditions are all true; otherwise, false.
     */
    bool populatedFileExists(const std::string& filename)
    {
      return std::filesystem::exists(POOLDIR + filename) &&
             std::filesystem::is_regular_file(POOLDIR + filename) &&
             std::filesystem::file_size(POOLDIR + filename) > 0;
    }

    /**
     * Checks if a filename contains a ".json" file extension.
     *
     * @param filename The filename to evaluate.
     * @returns True if the filename contains a ".json" file extension;
     * otherwise, false.
     */
    bool checkJSONFileExtenstion(const std::string& filename)
    {
      // Check if the filename is large enough to hold .json extension
      if (filename.size() <= 5) {
        sbmpi::util::Logger::getLogger().error("Filename length is incorrect!");
        return false;
      }

      // See if the filename includes .json extension
      int         fileExtIndex = filename.find_last_of(".");
      std::string fileExtension =
          filename.substr(fileExtIndex, filename.size());
      if (fileExtension.compare(".json") != 0) {
        sbmpi::util::Logger::getLogger().error(
            "Wallet file extension is not \'.json\'!");
        return false;
      }

      return true;
    }

    /**
     * @brief Generates a specified number of `Wallet` objects.

     * Returns a collection of populated `Wallet` objects, each with their
     "publicKey",
     * "privateKey", and "address" member variables populated. The values are
     all random
     * which is standard for most cryptocurrency implementations.
     * @param count Specifies how many `Wallet` instances to instantiate.
     * @return A std::vector containing popluated `Wallet` instances.
     */
    std::vector<sbmpi::core::state::Wallet> generateMockWallets(size_t count)
    {
      std::vector<sbmpi::core::state::Wallet> wallets;
      wallets.reserve(count);

      for (size_t i = 0; i < count; i++) {
        sbmpi::core::state::Wallet newWallet(
            true);  // Constructor populates member variables
        wallets.push_back(newWallet);
      }

      return wallets;
    }

    /**
     * @brief Writes a collection of `Wallet` object data to a specified JSON
     * file.
     *
     * The function needs to receive a file name with a .json extension or the
     * function will not output. If the file name is valid, then the function
     * iterates through the collection of `Wallet` objects and creates a
     * collection of JSON objects. Lastly, the final parsed JSON is written to
     * the desired file in the /pools directory.
     * @param filename A correct JSON filename.
     * @param wallets A vector instance containing populated `Wallet` objects.
     */
    void writeWalletsJSON(const std::string&                      filename,
                          std::vector<sbmpi::core::state::Wallet> wallets)
    {
      // Check filename for .json extension
      if (!checkJSONFileExtenstion(filename)) {
        return;
      }

      // Check if directory already exists, then gracefully continue
      mkdir(POOLDIR.c_str(), 0777);

      // Create a JSON containing a collection of wallet data
      json j;
      j["wallets"] = json::array();
      for (auto& wallet : wallets) {
        json jWallet = wallet.toJSON();
        j["wallets"].push_back(jWallet);
      }

      // Open in truncate mode, overwrite old run wallets (if needed)
      std::ofstream walletPool(POOLDIR + filename, std::ios::trunc);
      if (walletPool.is_open()) {
        walletPool << j.dump(4);
        walletPool.close();
      }
    }

    /**
     * @brief Reads a specified JSON file to create a vector of `Wallet`
     * objects.
     *
     * The function needs to receive a file name with a .json extension or the
     * function will not output. If the file name is valid, then the function
     * iterates through the collection of JSON wallet objects and instantiates a
     * collection of populated `Wallet` objects.
     * @param filename A correct JSON filename.
     */
    std::vector<sbmpi::core::state::Wallet> readWalletsJSON(
        const std::string& filename)
    {
      // Initialize a new wallet vector to store our new instances
      std::vector<sbmpi::core::state::Wallet> wallets;

      // Check if the filename for valid .json extension
      if (!checkJSONFileExtenstion(filename)) {
        // Throw an error if an invalid filename was provided
        throw std::runtime_error("Inalid file extension! Not a .json file.");
      }

      // Read the wallet JSON file and parse the fields into a new object
      std::ifstream walletPool(POOLDIR + filename);
      json          data = json::parse(walletPool);
      for (auto& wallet : data["wallets"]) {
        sbmpi::core::state::Wallet newWallet(false);
        newWallet.fromJSON(wallet);
        wallets.push_back(newWallet);
      }

      return wallets;
    }

    /**
     * @brief Generates a specified number of mock transactions.
     *
     * Transactions are generated with random senders, receivers (ensuring
     * different from sender), and amounts. A fixed seed is used for the random
     * number generator to ensure deterministic and repeatable results for
     * benchmarking. Each transaction is assigned a unique, deterministic ID and
     * a simulated signature. The first 10 generated transactions are
     * pretty-printed to stdout.
     *
     * @param count The number of mock transactions to generate.
     * @return A std::vector of generated core::state::Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count, std::vector<sbmpi::core::state::Wallet> wallets,
        double faultProbability)
    {
      std::vector<sbmpi::core::state::Transaction> transactions;
      transactions.reserve(count);

      // Use a fixed seed (42) for deterministic, repeatable results.
      // This ensures consistent benchmarking between serial and parallel runs.
      std::mt19937 gen(42);

      // Nonce tracking for each sender address
      std::unordered_map<std::string, uint64_t> nonces;

      // Simulate a pool of unique users
      std::uniform_int_distribution<> userDist(1, wallets.size());
      // Random transaction amounts between 0.01 and 1000.00
      std::uniform_real_distribution<> amountDist(0.01, 1000.0);
      // Distribution for fault injection
      std::uniform_real_distribution<> faultDist(0.0, 1.0);

      for (size_t i = 0; i < count; ++i) {
        // Generate Sender
        sbmpi::core::state::Wallet senderWallet = wallets.at(userDist(gen) - 1);
        std::string                senderAddress = senderWallet.address;
        // std::string sender = "user_" + std::to_string(userDist(gen));

        // Generate Receiver (ensure it is different from sender)
        sbmpi::core::state::Wallet receiverWallet(false);
        std::string                receiverAddress;
        do {
          // receiver = "user_" + std::to_string(userDist(gen));
          receiverWallet  = wallets.at(userDist(gen) - 1);
          receiverAddress = receiverWallet.address;
        } while (senderAddress == receiverAddress);

        double amount = amountDist(gen);

        // Get and increment the nonce for the sender
        uint64_t currentNonce = nonces[senderAddress]++;

        // Instantiate the Transaction
        sbmpi::core::state::Transaction tx(senderAddress, receiverAddress,
                                           amount, currentNonce);

        if (senderWallet.privateKeyRaw.empty()) {
          throw std::runtime_error("Empty private key!");
        }

        tx.sign(senderWallet.privateKeyRaw);

        if (faultDist(gen) < faultProbability) {
          tx.amount += 1000000.0;
          std::cout << "[GENERATOR] Injected FAULT into Tx: " << tx.id
                    << std::endl;
        }

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

    /**
     * @brief Writes a collection of `Transaction` object data to a specified
     * JSON file.
     *
     * The function needs to receive a file name with a .json extension or the
     * function will not output. If the file name is valid, then the function
     * iterates through the collection of `Transaction` objects and creates a
     * collection of JSON objects. Lastly, the final parsed JSON is written to
     * the desired file in the /pools directory.
     * @param filename A correct JSON filename.
     * @param wallets A vector instance containing populated `Transaction`
     * objects.
     */
    void writeTransactionsJSON(
        const std::string&                           filename,
        std::vector<sbmpi::core::state::Transaction> transactions)
    {
      if (!checkJSONFileExtenstion(filename)) {
        return;
      }

      // Check if directory already exists, then gracefully continue
      mkdir(POOLDIR.c_str(), 0777);

      // Create a JSON containing a collection of transaction data
      json j;
      j["transactions"] = json::array();
      for (auto& transaction : transactions) {
        json jTransaction = transaction.toJSON();
        j["transactions"].push_back(jTransaction);
      }

      // Open in truncate mode and overwrite existing transaction data (if
      // needed)
      std::ofstream transactionPool(POOLDIR + filename, std::ios::trunc);
      if (transactionPool.is_open()) {
        transactionPool << j.dump(4);
        transactionPool.close();
      }
    }

    /**
     * @brief Reads a specified JSON file to create a vector of `Transaction`
     * objects.
     *
     * The function needs to receive a file name with a .json extension or the
     * function will not output. If the file name is valid, then the function
     * iterates through the collection of JSON transaction objects and
     * instantiates a collection of populated `Transaction` objects.
     * @param filename A correct JSON filename.
     * @param transactions A vector instance containing populated `Transaction`
     * objects.
     */
    std::vector<sbmpi::core::state::Transaction> readTransactionsJSON(
        const std::string& filename)
    {
      // Initialize a new transaction vector to store our new instances
      std::vector<sbmpi::core::state::Transaction> transactions;

      // Check if the filename for valid .json extension
      if (!checkJSONFileExtenstion(filename)) {
        // Throw an error if an invalid filename was provided
        throw std::runtime_error("Inalid file extension! Not a .json file.");
      }

      // Read the transaction JSON file and parse the fields into a new object
      std::ifstream transactionPool(POOLDIR + filename);
      json          data = json::parse(transactionPool);
      for (auto& tx : data["transactions"]) {
        sbmpi::core::state::Transaction newTx;
        newTx.fromJSON(tx);
        transactions.push_back(newTx);
      }

      return transactions;
    }

  }  // namespace util
}  // namespace sbmpi

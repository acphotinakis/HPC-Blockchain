#ifndef SBMPI_GENERATOR_H
#define SBMPI_GENERATOR_H

#include <vector>
#include <filesystem>
#include "../core/state/transaction.h"
#include "../core/state/wallet.h"

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
    bool populatedFileExists(const std::string& filename);

    /**
     * Checks if a filename contains a ".json" file extension.
     *
     * @param filename The filename to evaluate.
     * @returns True if the filename contains a ".json" file extension; otherwise, false.
     */
    bool checkJSONFileExtenstion(const std::string& filename);

    /**
     * @brief Generates a set of mock user wallets. 
     *
     * * This function ensures all private keys, public keys, and addresses are 
     * random. 
     * * @param count The number of wallets to generate.
     * @return A vector of fully populated `Wallet` objects.
     */
    std::vector<sbmpi::core::state::Wallet> generateMockWallets(size_t count);
    
    /**
     * @brief Writes a collection of `Wallet` objects to a specified file.
     *
     * * This function write the contents of each `Wallet` object to a specified JSON
     * file. Each of the object's attributes are stored in a collection of JSON objects.
     * * @param filename The file name to write the data to. (Must have .json suffix).
     * * @param wallets A collection of populated `Wallet` objects.
     */
    void writeWalletsJSON(const std::string& filename, std::vector<sbmpi::core::state::Wallet> wallets);

    /**
     * @brief Reads a specified JSON file to create a vector of `Wallet` objects.
     *
     * * This function reads the contents of a specified ".json" file. Returns a new 
     * vector of `Wallet` objects.
     * @param filename The file name to read the data from. (Must have .json suffix).
     */
    std::vector<sbmpi::core::state::Wallet> readWalletsJSON(const std::string& filename);
    
    /**
     * @brief Generates a deterministic set of mock transactions.
     * * This function uses a fixed random seed to ensure that the workload
     * is identical across different simulation runs, allowing for accurate
     * benchmarking of the sharding speedup.
     * * @param count The number of transactions to generate.
     * @return A vector of fully populated and signed Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count, std::vector<sbmpi::core::state::Wallet> wallets, double faultProbability = 0.0);
    
    /**
     * @brief Writes a collection of `Transaction` objects to a specified file.
     *
     * * This function write the contents of each `Transaction` object to a specified JSON
     * file. Each of the object's attributes are stored in a collection of JSON objects.
     * * @param filename The file name to write the data to. (Must have .json suffix).
     */
    void writeTransactionsJSON(
      const std::string& filename, std::vector<sbmpi::core::state::Transaction> transactions);
    
    /**
     * @brief Reads a specified JSON file to create a vector of `Transaction` objects.
     *
     * * This function reads the contents of a specified ".json" file. Returns a new 
     * vector of `Transaction` objects.
     * * @param filename The file name to read the data from. (Must have .json suffix).
     * * @param wallets A collection of populated `Wallet` objects.
     */
      std::vector<sbmpi::core::state::Transaction> readTransactionsJSON(
      const std::string& filename);

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_GENERATOR_H
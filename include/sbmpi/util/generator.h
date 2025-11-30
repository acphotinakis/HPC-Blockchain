#ifndef SBMPI_GENERATOR_H
#define SBMPI_GENERATOR_H

#include <vector>
#include "../core/state/transaction.h"
#include "../core/state/wallet.h"

namespace sbmpi
{
  namespace util
  {
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
    void writeWalletAddresses(const std::string& filename, std::vector<sbmpi::core::state::Wallet> wallets);
    
    /**
     * @brief Generates a deterministic set of mock transactions.
     * * This function uses a fixed random seed to ensure that the workload
     * is identical across different simulation runs, allowing for accurate
     * benchmarking of the sharding speedup.
     * * @param count The number of transactions to generate.
     * @return A vector of fully populated and signed Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count, std::vector<sbmpi::core::state::Wallet> wallets);

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_GENERATOR_H
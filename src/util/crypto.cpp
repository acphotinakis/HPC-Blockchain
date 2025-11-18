#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include <openssl/sha.h>
#include <vector>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <string>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    std::string sha256(const std::string& data)
    {
      unsigned char hash[SHA256_DIGEST_LENGTH];
      SHA256_CTX    sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256, data.c_str(), data.size());
      SHA256_Final(hash, &sha256);
      std::stringstream ss;
      for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
      }
      return ss.str();
    }

    std::string sign(const std::string& data, const std::string& privateKey)
    {
      // Dummy implementation
      return sha256(data + privateKey);
    }

    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey)
    {
      // Dummy implementation
      return signature == sha256(data + publicKey);
    }

    /**
    * @brief Calculates the Merkle root hash given a vector of 'Transactions'.
    * @param transactions A reference to a std::vector containing core::state:Transaction instances.
    * @return A std::string representation of the calculate Merkle root. 
    */
    std::string merkle(const std::vector<core::state::Transaction>& transactions)
    {
      // Cannot compute merkle root with empty set of TXs
      if (transactions.empty()) 
      {
        return "";
      }

      // Create a vector containing the IDs (hashes) of the TXs
      std::vector<std::string> currentTransactions;
      for (const auto& tx : transactions) {
        // Debug
        //std::cout << "TX:[" << tx.id << "]" << std::endl;
        currentTransactions.push_back(tx.id);
      }

      // Combine the hashes until the merkle root is reached
      while (currentTransactions.size() != 1)
      {
        // Create a new vector containing the combined hashes
        std::vector<std::string> newTransactions;
        // Iterate the current vector of transactions by steps of 2
        for (size_t i = 0; i < currentTransactions.size(); i+=2)
        {
          // If two hashes can be accessed, then hash the combination of the two neighboring hashes
          if (i+1 < currentTransactions.size())
          {
            std::string newHash = sha256(currentTransactions[i] + currentTransactions[i+1]);
            newTransactions.push_back(newHash);
          }
          // Otherwise, add the single edge hash to the new vector of hashes
          else
          {
            newTransactions.push_back(currentTransactions[i]);
          }
        }
        // Reference the new vector at the end of iterations
        currentTransactions = newTransactions;
      }

      // Return the merkle root
      return currentTransactions[0];
    }

  }  // namespace util
}  // namespace sbmpi

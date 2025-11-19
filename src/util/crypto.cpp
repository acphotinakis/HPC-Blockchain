#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include "../../include/sbmpi/util/logging.h"
#include <openssl/evp.h>
#include <vector>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <string>
#include <charconv>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    std::string logCryptoError(const std::string& errorMessage)
    {
      auto& logger = util::Logger::getLogger();
      logger.log(util::LogLevel::ERROR, errorMessage);
      return "";
    }

    std::string sha256(const std::string& message)
    {
        EVP_MD_CTX *context = EVP_MD_CTX_new();
        if (!context)
            return logCryptoError("Error initializing EVP context!");

        if (EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1)
            return logCryptoError("Error initializing SHA256 digest!");

        if (EVP_DigestUpdate(context, message.data(), message.size()) != 1)
            return logCryptoError("Error updating SHA256 digest!");

        unsigned char *digest = (unsigned char*)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
        if (!digest)
            return logCryptoError("Error allocating digest buffer!");

        unsigned int digest_len = 0;
        if (EVP_DigestFinal_ex(context, digest, &digest_len) != 1)
            return logCryptoError("Error producing SHA256 final digest!");

        EVP_MD_CTX_free(context);

        std::stringstream ss;
        ss << std::hex << std::setfill('0'); // Define stream as hex and fill any spaces with zeroes
        for (unsigned int i = 0; i < digest_len; i++)
            ss << std::setw(2) << (int)digest[i]; // Need std::setw(2) to define a hex number with format XX

        OPENSSL_free(digest);
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

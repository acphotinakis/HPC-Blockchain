#include "../../include/sbmpi/util/crypto.h"
#include <openssl/evp.h>  // Use EVP header instead of sha.h
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace util
  {

    std::string sha256(const std::string& data)
    {
      unsigned char hash[EVP_MAX_MD_SIZE];
      unsigned int  lengthOfHash = 0;

      EVP_MD_CTX* context = EVP_MD_CTX_new();

      if (context != nullptr) {
        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr)) {
          if (EVP_DigestUpdate(context, data.c_str(), data.size())) {
            EVP_DigestFinal_ex(context, hash, &lengthOfHash);
          }
        }
        EVP_MD_CTX_free(context);
      }

      std::stringstream ss;
      for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
      }
      return ss.str();
    }

    std::string sign(const std::string& data, const std::string& privateKey)
    {
      // Dummy implementation: Signature = SHA256(Data + PrivateKey)
      return sha256(data + privateKey);
    }

    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey)
    {
      // FIX: Align verification with the generator's signing logic.
      // The generator signs with "private_key_" + sender.
      // The transaction passes 'sender' (the address) as 'publicKey'.
      // So to verify, we must reconstruct the signing key used by the mock
      // generator.

      // In a real system, we would mathematically verify (Sig, Data, PubKey).
      // In this simulation, we check: Is Sig == SHA256(Data + "private_key_" +
      // PubKey)?
      std::string expectedPrivateKey = "private_key_" + publicKey;

      return signature == sha256(data + expectedPrivateKey);
    }

    /**
     * @brief Calculates the Merkle root hash given a vector of 'Transactions'.
     * @param transactions A reference to a std::vector containing
     * core::state:Transaction instances.
     * @return A std::string representation of the calculate Merkle root.
     */
    std::string merkle(
        const std::vector<core::state::Transaction>& transactions)
    {
      // Cannot compute merkle root with empty set of TXs
      if (transactions.empty()) {
        return "";
      }

      // Create a vector containing the IDs (hashes) of the TXs
      std::vector<std::string> currentTransactions;
      for (const auto& tx : transactions) {
        currentTransactions.push_back(tx.id);
      }

      // Combine the hashes until the merkle root is reached
      while (currentTransactions.size() != 1) {
        // Create a new vector containing the combined hashes
        std::vector<std::string> newTransactions;
        // Iterate the current vector of transactions by steps of 2
        for (size_t i = 0; i < currentTransactions.size(); i += 2) {
          // If two hashes can be accessed, then hash the combination of the two
          // neighboring hashes
          if (i + 1 < currentTransactions.size()) {
            std::string newHash =
                sha256(currentTransactions[i] + currentTransactions[i + 1]);
            newTransactions.push_back(newHash);
          }
          // Otherwise, add the single edge hash to the new vector of hashes
          else {
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

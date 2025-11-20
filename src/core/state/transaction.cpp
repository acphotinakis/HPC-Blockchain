/**
 * @file transaction.cpp
 * @brief Implements the Transaction class for representing blockchain transactions.
 */
#include "../../../include/sbmpi/core/state/transaction.h"

#include <chrono>
#include <sstream>
#include <vector>

#include "../../../include/sbmpi/util/crypto.h"
#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Represents a single transaction in the blockchain.
       *
       * A transaction includes sender, receiver, amount, a unique ID, and a digital signature.
       * It provides methods for signing, verifying, serializing, and deserializing.
       */
      Transaction::Transaction() : amount(0.0) {}

      /**
       * @brief Constructs a new Transaction object.
       *
       * Generates a unique transaction ID based on sender, receiver, amount, and timestamp.
       * @param from The sender's address.
       * @param to The receiver's address.
       * @param amount The amount of currency to transfer.
       */
      Transaction::Transaction(const std::string& from, const std::string& to,
                               double amount)
          : from(from), to(to), amount(amount)
      {
        std::stringstream ss;
        ss << from << to << amount
           << std::chrono::system_clock::now().time_since_epoch().count();
        id = util::sha256(ss.str());
      }

      /**
       * @brief Signs the transaction using a provided private key.
       *
       * The signature is a hash of the transaction data combined with the private key.
       * (Note: This is a simplified simulation of signing for demonstration purposes).
       * @param privateKey The private key used to sign the transaction.
       */
      void Transaction::sign(const std::string& privateKey)
      {
        std::string data = from + to + std::to_string(amount);
        signature = util::sign(data, privateKey);
      }

      /**
       * @brief Verifies the transaction's signature.
       *
       * Checks if the signature is valid for the transaction data and the sender's public key.
       * (Note: This verification is simplified for simulation and assumes 'from' is the public key).
       * @return True if the signature is valid, false otherwise.
       */
      bool Transaction::verify() const
      {
        std::string data = from + to + std::to_string(amount);
        // Assumes the 'from' address is the public key for dummy verification
        return util::verify(data, signature, from);
      }

      /**
       * @brief Serializes the Transaction object into a vector of characters.
       *
       * The serialization includes the transaction ID, sender, receiver, amount, and signature.
       * @return A std::vector<char> containing the serialized transaction data.
       */
      std::vector<char> Transaction::serialize() const
      {
        std::vector<char> buffer;
        util::pack(id, buffer);
        util::pack(from, buffer);
        util::pack(to, buffer);
        util::pack(amount, buffer);
        util::pack(signature, buffer);
        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a Transaction object.
       *
       * Reconstructs the Transaction from its serialized byte representation.
       * @param data The std::vector<char> containing the serialized transaction data.
       */
      void Transaction::deserialize(const std::vector<char>& data)
      {
        int offset = 0;
        id = util::unpack_string(data, offset);
        from = util::unpack_string(data, offset);
        to = util::unpack_string(data, offset);
        amount = util::unpack_double(data, offset);
        signature = util::unpack_string(data, offset);
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

/**
 * @file transaction.cpp
 * @brief Implements the Transaction class for representing blockchain
 * transactions.
 */
#include "../../../include/sbmpi/core/state/transaction.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

#include "../../../include/sbmpi/util/crypto.h"
#include "../../../include/sbmpi/util/logging.h"
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
       * A transaction includes sender, receiver, amount, a unique ID, and a
       * digital signature. It provides methods for signing, verifying,
       * serializing, and deserializing.
       */
      Transaction::Transaction() : amount(0.0), nonce(0) {}

      /**
       * @brief Constructs a new Transaction object.
       *
       * Generates a unique transaction ID based on sender, receiver, amount,
       * and timestamp.
       * @param from The sender's address.
       * @param to The receiver's address.
       * @param amount The amount of currency to transfer.
       */
      Transaction::Transaction(const std::string& from, const std::string& to,
                               double amount, uint64_t nonce)
          : from(from), to(to), amount(amount), nonce(nonce)
      {
        time = std::chrono::system_clock::now().time_since_epoch().count();

        rawId = constructHash();
        if (rawId.size() != util::KEYLEN) {
          throw std::runtime_error("ID is not of correct length!");
        }
        id = util::toHex(rawId);
      }

      /**
       * @brief Produces a Keccak-256 (SHA3-256) hash of transaction data.
       *
       * Generates a unique hash based on sender, receiver, amount, and
       * timestamp.
       */
      std::vector<unsigned char> Transaction::constructHash() const
      {
        std::stringstream ss;
        ss << from << "|" << to << "|" << amount << "|" << nonce << "|" << time;
        // Set the ID to the Keccak256 hash
        std::string                hashStr = ss.str();
        std::vector<unsigned char> hashBytes(hashStr.begin(), hashStr.end());

        return util::keccak256(hashBytes);
      }

      /**
       * @brief Signs the transaction using a provided private key.
       *
       * The signature uses ECDSA to compute a recoverable signature using
       * transaction data and a private key.
       * @param privateKey The private key used to sign the transaction.
       */
      void Transaction::sign(const std::vector<unsigned char>& privateKey)
      {
        signatureRaw = util::sign(rawId, privateKey);
        signature    = util::toHex(signatureRaw);

        if (signatureRaw.empty() ||
            signatureRaw.size() != (util::KEYLEN * 2) + 1) {
          throw std::runtime_error(
              "[CRYPTO]: Signature is empty or incorrect size!");
        }
      }

      /**
       * @brief Verifies the transaction's signature.
       *
       * Checks if the signature is valid for the transaction data and the
       * sender's public key.
       * @return True if the signature is valid, false otherwise.
       */
      bool Transaction::verify() const
      {
        // Check if our addresses are populated
        if (from.empty() || to.empty()) {
          util::Logger::getLogger().error("Empty from and to.");
          return false;
        }

        std::vector<unsigned char> newId = constructHash();
        return util::verify(newId, signatureRaw);
      }

      /**
       * @brief Serializes transaction data to a `json` instance.
       *
       * Uses Niels Lohmann's C++ JSON library for straightforward JSON parsing.
       * Fields to write: "id", "from", "to", "amount", "time", and "signature".
       * @return A `json` instance containing transaction data in JSON format.
       */
      nlohmann::json Transaction::toJSON() const
      {
        json transactionJson = {{"id", id},
                                {"from", from},
                                {"to", to},
                                {"amount", amount},
                                {"time", time},
                                {"nonce", nonce},
                                {"signature", signature}};
        return transactionJson;
      }

      /**
       * @brief Deserializes `json` data to populate a Transaction's data.
       *
       * Uses Niels Lohmann's C++ JSON library for straightforward JSON reading.
       * Fields to read: "id", "from", "to", "amount", "time", and "signature".
       */
      void Transaction::fromJSON(json& json)
      {
        id    = json["id"].get<std::string>();
        rawId = util::hexToBytes(id);

        from   = json["from"].get<std::string>();
        to     = json["to"].get<std::string>();
        amount = json["amount"].get<double>();
        time   = json["time"].get<int64_t>();
        nonce  = json["nonce"].get<uint64_t>();

        signature    = json["signature"].get<std::string>();
        signatureRaw = util::hexToBytes(signature);
      }

      /**
       * @brief Serializes the Transaction object into a vector of characters.
       *
       * The serialization includes the transaction ID, sender, receiver,
       * amount, and signature.
       * @return A std::vector<char> containing the serialized transaction data.
       */
      std::vector<char> Transaction::serialize() const
      {
        std::vector<char> buffer;
        util::pack(id, buffer);
        util::pack(rawId, buffer);
        util::pack(from, buffer);
        util::pack(to, buffer);
        util::pack(amount, buffer);
        util::pack_int64(time, buffer);
        util::pack_uint64(nonce, buffer);
        util::pack(signature, buffer);
        util::pack(signatureRaw, buffer);
        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a Transaction object.
       *
       * Reconstructs the Transaction from its serialized byte representation.
       * @param data The std::vector<char> containing the serialized transaction
       * data.
       */
      void Transaction::deserialize(const std::vector<char>& data)
      {
        int offset   = 0;
        id           = util::unpack_string(data, offset);
        rawId        = util::unpack_vector_unsigned_char(data, offset);
        from         = util::unpack_string(data, offset);
        to           = util::unpack_string(data, offset);
        amount       = util::unpack_double(data, offset);
        time         = util::unpack_int64_t(data, offset);
        nonce        = util::unpack_uint64_t(data, offset);
        signature    = util::unpack_string(data, offset);
        signatureRaw = util::unpack_vector_unsigned_char(data, offset);
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

/**
 * @file crypto.cpp
 * @brief Provides cryptographic utility functions for hashing, signing, and verifying.
 *
 * This file implements SHA-256 hashing using OpenSSL, and dummy implementations
 * for digital signing and verification for simulation purposes. It also includes
 * a Merkle tree root calculation function.
 */
#include "../../include/sbmpi/util/crypto.h"
#include <openssl/evp.h>  // Use EVP header instead of sha.h
#include <openssl/rand.h>
#include "secp256k1.h"
#include "secp256k1_recovery.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include "../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace util
  {
    /**
    * @brief Converts a byte array into a hexadecimal string.
    * @param data Pointer to the byte array to convert.
    * @param len Length of the byte array.
    * @return Hexadecimal representation of the input data as a string.
    */
    std::string toHex(const std::vector<unsigned char>& data) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < data.size(); i++)
            ss << std::setw(2) << (int)data.at(i);
        return ss.str();
    }

    /**
    * @brief Generates a new random 32-byte Ethereum private key.
    * 
    * Uses OpenSSL's RAND_bytes to generate secure random bytes.
    * @return An array of 32 unsigned char representing the generated private key.
    */
    std::vector<unsigned char> generatePrivateKey() {
      std::vector<unsigned char> bytes(KEYLEN);
      if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
          throw std::runtime_error("[CRYPTO]: Failed to generate secure private key!");
      }
      return bytes;
    }

    /**
    * @brief Derives the uncompressed public key from a given private key.
    * 
    * Uses secp256k1 (from the downloaded source) to compute the corresponding 
    * public key. The resulting public key is serialized in uncompressed format 
    * (65 bytes, starting with 0x04, indicating an uncompressed key).
    * @param privateKey A 32-byte Ethereum private key.
    * @return The serialized uncompressed public key.
    * @throws std::runtime_error If the private key is invalid.
    */
    std::vector<unsigned char> derivePublicKey(const std::vector<unsigned char>& privateKey) {
      // Inititalize the crypto context using secp256k1
      secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

      secp256k1_pubkey publicKey;
      if (!secp256k1_ec_pubkey_create(ctx, &publicKey, privateKey.data())) {
        throw std::runtime_error("Invalid private key");
      }

      // First bytes to denote uncompressed key, next 32 bytes for x-coord in elliptic key cryptography,
      // last 32 bytes for the y-coord.
      size_t publicKeyLen = (KEYLEN*2) + 1; 
      std::vector<unsigned char> publicKeySerialized(publicKeyLen);
      secp256k1_ec_pubkey_serialize(
          ctx,
          publicKeySerialized.data(),
          &publicKeyLen,
          &publicKey,
          SECP256K1_EC_UNCOMPRESSED
      );
      secp256k1_context_destroy(ctx);

      return publicKeySerialized;
    }

    /**
    * @brief Computes the Keccak-256 (SHA3-256) hash of the given data.
    * 
    * Uses OpenSSL EVP to compute the hash.
    * @param data Pointer to the input data.
    * @param len Length of the input data in bytes.
    * @return The 32-byte Keccak-256 (SHA3-256) hash.
    * @throws std::runtime_error If the hash length is not 32 bytes.
    */
    std::vector<unsigned char> keccak256(const std::vector<unsigned char>& data) {
        std::vector<unsigned char> hash(KEYLEN);
        unsigned int lengthOfHash = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();

        if (ctx != nullptr) {
          if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr)) {
            if (EVP_DigestUpdate(ctx, data.data(), data.size()) == 1) {
              EVP_DigestFinal_ex(ctx, hash.data(), &lengthOfHash);
            }
          }
          EVP_MD_CTX_free(ctx);
        }

        if (lengthOfHash != hash.size())
            throw std::runtime_error("Unexpected hash length");

        return hash;
    }

    /**
    * @brief Derives an Ethereum address from a given uncompressed public key.
    * 
    * The address is the last 20 bytes of the Keccak-256 (SHA3-256) hash of the public key
    * (excluding the 0x04 prefix).
    * @param publicKey Serialized uncompressed public key (65 bytes, starting with 0x04).
    * @return The derived Ethereum hexadecimal address.
    */
    std::string deriveAddress(const std::vector<unsigned char>& publicKey) {
      // Copy the public key into stack buffer
      unsigned char hashBuffer[(KEYLEN*2) + 1];
      std::copy(publicKey.begin(), publicKey.end(), hashBuffer);

      // Convert slice (skip 0x04) into a vector
      std::vector<unsigned char> pubSlice(hashBuffer + 1, hashBuffer + publicKey.size());

      // Hash the 64-byte (x||y)
      auto hash = keccak256(pubSlice);

      // FIX: Extract last 20 bytes from the HASH, not from hashBuffer
      std::vector<unsigned char> last20(hash.end() - 20, hash.end());
      
      return toHex(last20);
    }

    std::string recoverAddress(
      const std::vector<unsigned char>& signature, const std::vector<unsigned char>& hash
    ) {
      // Create a SECP256k1 verification context
      secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);

      // Extract our X||Y streams
      std::vector<unsigned char> recoveryHash(signature.begin(), signature.end() - 1);
      int recoveryId = static_cast<int>(signature.back());

      secp256k1_ecdsa_recoverable_signature sig;
      if (!secp256k1_ecdsa_recoverable_signature_parse_compact(
        ctx, &sig, recoveryHash.data(), recoveryId
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Could not parse recoverable signature!");
      }

      secp256k1_pubkey publicKey;
      if (!secp256k1_ecdsa_recover(
        ctx, &publicKey, &sig, hash.data()
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Could not recover public key from signature!");
      }

      size_t publicKeyLen = (KEYLEN*2) + 1;
      unsigned char publicKeySerialized[publicKeyLen];
      secp256k1_ec_pubkey_serialize(
        ctx, 
        publicKeySerialized, 
        &publicKeyLen, 
        &publicKey, 
        SECP256K1_EC_UNCOMPRESSED
      );

      std::vector<unsigned char> publicKeySlice(publicKeySerialized + 1, publicKeySerialized + publicKeyLen);
      std::vector<unsigned char> recoveredHash = keccak256(publicKeySlice);
      std::vector<unsigned char> address(recoveredHash.end() - 20, recoveredHash.end());
      return toHex(address);
    }

    /**
     * @brief Computes the SHA-256 hash of a given string.
     * @param data The input string to be hashed.
     * @return A std::string representing the hexadecimal SHA-256 hash.
     */
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

      // Using vector to maintain standard
      std::vector<unsigned char> hashVector;
      hashVector.reserve(lengthOfHash);
      for (unsigned char c: hash) {
        hashVector.push_back(c);
      }

      return toHex(hashVector);
    }

    /**
     * @brief Generates a dummy digital signature for given data using a private key.
     *
     * For simulation purposes, the signature is simply the SHA-256 hash of
     * the concatenated data and private key. In a real system, this would
     * involve asymmetric cryptography.
     * @param data The data to be signed.
     * @param privateKey The private key used for signing.
     * @return A std::string representing the dummy signature.
     */
    std::vector<unsigned char> sign(const std::vector<unsigned char>& hash, 
      const std::vector<unsigned char>& privateKey)
    {
      if (privateKey.size() != KEYLEN) {
        throw std::runtime_error("Private key size is not of correct length!");
      }

      secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

      // We want to be able to recover the public key during verification
      secp256k1_ecdsa_recoverable_signature sig;
      if (!secp256k1_ecdsa_sign_recoverable(
        ctx, &sig, 
        hash.data(), privateKey.data(),
        nullptr, nullptr
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Failed to sign transaction");
      }

      unsigned char sigCompact[KEYLEN*2];
      int recoveryId;
      if (!secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx, sigCompact, &recoveryId, &sig
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Failed to compact signature!");
      }

      secp256k1_context_destroy(ctx);

      std::vector<unsigned char> signature(sigCompact, sigCompact + (KEYLEN*2));
      signature.push_back(static_cast<unsigned char>(recoveryId));
      return signature;
    }

    /**
     * @brief Verifies a dummy digital signature against data and a public key.
     *
     * For simulation purposes, this checks if the provided signature matches
     * the SHA-256 hash of the data concatenated with a reconstructed "private key"
     * based on the public key (as used by the mock generator).
     * @param data The original data that was signed.
     * @param signature The signature to verify.
     * @param publicKey The public key (in this simulation, the sender's address).
     * @return True if the signature is valid, false otherwise.
     */
    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey)
    {
      // Align verification with the generator's signing logic.
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
     *
     * This function constructs a Merkle tree from the transaction IDs (hashes)
     * and returns the root hash. If the number of transactions is odd, the last
     * hash is duplicated at each level.
     * @param transactions A reference to a std::vector containing
     * core::state::Transaction instances.
     * @return A std::string representation of the calculated Merkle root, or an
     *         empty string if the input vector is empty.
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

  } // namespace util
} // namespace sbmpi

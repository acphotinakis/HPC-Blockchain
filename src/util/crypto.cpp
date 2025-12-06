/**
 * @file crypto.cpp
 * @brief Provides cryptographic utility functions for hashing, signing, and verifying.
 *
 * This file implements Keccak-256 hashing using OpenSSL and ECDSA functionality 
 * for digital signing and verification using secp256k1 for simulation purposes.
 * It also includes a Merkle tree root calculation function.
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
#include "../../include/sbmpi/util/logging.h"

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
    * @brief Generates a valid EC 32-byte Ethereum private key.
    * 
    * Uses OpenSSL's RAND_bytes function to generate secure random bytes.
    * @return An array of 32 unsigned char representing the generated private key.
    */
    std::vector<unsigned char> generatePrivateKey(const secp256k1_context* ctx) {
      // Create a new secp256k1 verify context if not already instantiated
      if (ctx == nullptr) {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
      }

      // Ensure that the private key is less than the secp256k1 curve order
      std::vector<unsigned char> bytes(KEYLEN);
      do {
        if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
            throw std::runtime_error("[CRYPTO]: Failed to generate secure private key!");
        }
      } while (!secp256k1_ec_seckey_verify(ctx, bytes.data()));
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
      // Instantiate a new vector of (32 bytes)
      std::vector<unsigned char> hash(KEYLEN);
      unsigned int lengthOfHash = 0;
      EVP_MD_CTX* ctx = EVP_MD_CTX_new();

      // Hash the data using SHA3-256 (Keccak-256)
      if (ctx != nullptr) {
        if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr)) {
          if (EVP_DigestUpdate(ctx, data.data(), data.size()) == 1) {
            EVP_DigestFinal_ex(ctx, hash.data(), &lengthOfHash);
          }
        }
        EVP_MD_CTX_free(ctx);
      }

      // Verify that the generated hash fills the byte vector
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

    /**
     * @brief Generates a digital signature for given data using a private key.
     *
     * Uses the secp256k1 library for ECDSA operations.
     * Given a valid private key was provided, this function constructs a recoverable ECDSA signature 
     * using the computed Keccak-256 hash of transaction data and a private key. The function returns the 
     * signature as a vector containing unsigned chars representing hexadecimal bytes. The first 64 bytes 
     * of the vector represent the signature, where the last byte is the recovery ID used in public key recovery 
     * and verification.
     * @param data The data to be signed.
     * @param privateKey The private key used for signing.
     * @return A std::vector<unsigned char> representing the signature + recovery ID in hexadecimal bytes.
     */
    std::vector<unsigned char> sign(const std::vector<unsigned char>& data, 
      const std::vector<unsigned char>& privateKey)
    {
      // Ensure that the private key is correct length
      if (privateKey.size() != KEYLEN) {
        throw std::runtime_error("Private key size is not of correct length!");
      }

      // Create a secp256k1 sign context
      secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

      // Verify that the private key is a valid EC private key less than secp256k1 curve order
      if (!secp256k1_ec_seckey_verify(ctx, privateKey.data())) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Invalid EC private key!");
      }

      // Create a recoverable signature (essential for verifying) instance for the initial signature
      secp256k1_ecdsa_recoverable_signature sig;
      if (!secp256k1_ecdsa_sign_recoverable(
        ctx, &sig, 
        data.data(), privateKey.data(),
        nullptr, nullptr
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Failed to sign transaction");
      }

      // Now serialize the secp256k1 signature into bytes, and extract recovery ID
      unsigned char sigCompact[KEYLEN*2];
      int recoveryId;
      if (!secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx, sigCompact, &recoveryId, &sig
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Failed to compact signature!");
      }

      secp256k1_context_destroy(ctx);

      // Concatenate the signature (first 64 bytes), and recovery ID (last byte)
      std::vector<unsigned char> signature(sigCompact, sigCompact + (KEYLEN*2));
      signature.push_back(static_cast<unsigned char>(recoveryId));
      return signature;
    }

    /**
     * @brief Verifies a digital signature against data and a public key.
     *
     * Uses the secp256k1 library for ECDSA operations. 
     * Given that valid data and signature bytes were provided, this function starts by recovering 
     * the public key from the recoverable signature and data. Then, the signature is parsed and 
     * converted to a suitable lower-S format. Finally, the signature validity is returned using the 
     * secp256k1 ECDSA verify function.
     * @param data The hashed transaction data used for verification.
     * @param signature The signature to verify.
     * @return True if the signature is valid, false otherwise.
     */
    bool verify(
      const std::vector<unsigned char>& data, 
      const std::vector<unsigned char>& signature
    )
    {
      // Check to see if the signature and data are not empty
      if (signature.empty()) {
        util::Logger::getLogger().error("Empty signature.");  
        return false;
      }

      if (data.empty()) {
        util::Logger::getLogger().error("Empty ID.");  
        return false;
      }

      // Check if the signature and data are correct lengths
      if (signature.size() < (util::KEYLEN*2) + 1 || data.size() < util::KEYLEN) {
        util::Logger::getLogger().error("Incorrect key lengths.");  
        return false;
      }

      // Create an secp256k1 verify context
      secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);

      // Since the signature parameter includes 65 bytes, extract the signature (64 bytes),
      // and the recovery ID (last byte)
      std::vector<unsigned char> signatureBytes(signature.begin(), signature.end() - 1);
      int recoveryId = static_cast<int>(signature.back());

      // Parse a compact version of a secp256k1 recoverable signature
      secp256k1_ecdsa_recoverable_signature recoverSig;
      if (!secp256k1_ecdsa_recoverable_signature_parse_compact(
        ctx, &recoverSig, signatureBytes.data(), recoveryId
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Could not parse recoverable signature!");
      }

      // After creating the recoverable signature instance, recover the public key used in verification
      secp256k1_pubkey publicKey;
      if (!secp256k1_ecdsa_recover(
        ctx, &publicKey, &recoverSig, data.data()
      )) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Could not recover public key from signature!");
      }

      // A normal signature is required in the verify function, so convert recoverable to normal
      secp256k1_ecdsa_signature normalSig;
      if (!secp256k1_ecdsa_recoverable_signature_convert(ctx, &normalSig, &recoverSig)) {
        secp256k1_context_destroy(ctx);
        throw std::runtime_error("[CRYPTO]: Could not convert recoverable signature!");
      }

      // The verify function also requires a normalize signature (lower-S format)
      secp256k1_ecdsa_signature sigNormalized;
      secp256k1_ecdsa_signature_normalize(ctx, &sigNormalized, &normalSig);

      // Finally verify the parsed and converted signature for signature validity with respect
      // to the passed data.
      bool isValid = secp256k1_ecdsa_verify(ctx, &sigNormalized, data.data(), &publicKey);

      secp256k1_context_destroy(ctx);

      return isValid;

    }

    /**
     * @brief Calculates the Merkle root hash given a vector of 'Transaction' 
     * instances.
     *
     * This function constructs a Merkle tree from the transaction IDs (hashes)
     * and returns the root hash. If the number of transactions is odd, the last
     * hash is duplicated at each level.
     * @param transactions A reference to a std::vector containing
     * core::state::Transaction instances.
     * @return A std::string representation of the calculated Merkle root, or an
     * empty string if the input vector is empty.
     */
    std::string merkle(
        const std::vector<core::state::Transaction>& transactions)
    {
      std::vector<std::vector<unsigned char>> currentTransactions;

      // Cannot compute merkle root with empty set of TXs
      if (transactions.empty()) {
        return "";
      }

      // Create a vector containing the IDs (hashes) of the TXs
      for (const auto& tx : transactions) {
        currentTransactions.push_back(tx.rawId);
      }

      // Combine the hashes until the merkle root is reached
      while (currentTransactions.size() != 1) {
        // Create a new vector containing the combined hashes
        std::vector<std::vector<unsigned char>> newTransactions;
        // Iterate the current vector of transactions by steps of 2
        for (size_t i = 0; i < currentTransactions.size(); i += 2) {
          // If two hashes can be accessed, then hash the combination of the two
          // neighboring hashes
          if (i + 1 < currentTransactions.size()) {
            // Concatenate the two hashes into a separate byte vector
            std::vector<unsigned char> hashBytes;
            hashBytes.insert(hashBytes.end(), currentTransactions[i].begin(), currentTransactions[i].end());
            hashBytes.insert(hashBytes.end(), currentTransactions[i + 1].begin(), currentTransactions[i + 1].end());

            // Compute the Keccak256 hash of the concatenated hashes
            std::vector<unsigned char> newHash = keccak256(hashBytes);
            newTransactions.push_back(newHash);
          }
          // Otherwise, add the edge hash to the new vector of hashes
          else {
            newTransactions.push_back(currentTransactions[i]);
          }
        }
        // Reference the new vector at the end of iterations
        currentTransactions = newTransactions;
      }

      // Return the merkle root as a hex string
      return toHex(currentTransactions[0]);
    }

  } // namespace util
} // namespace sbmpi

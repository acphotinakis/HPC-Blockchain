#ifndef SBMPI_CRYPTO_H
#define SBMPI_CRYPTO_H

#include <string>

/**
 * @file crypto.h
 * @brief Provides interfaces for cryptographic operations.
 *
 * This file declares functions for hashing, digital signatures, and
 * verification, which are essential for ensuring the integrity and authenticity
 * of transactions and blocks. The implementations in `src/util/crypto.cpp` will
 * likely use a third-party cryptography library (e.g., OpenSSL).
 */

namespace sbmpi
{
  namespace crypto
  {

    /**
     * @brief Computes the SHA-256 hash of a string.
     *
     * @param data The input data to hash.
     * @return The resulting 32-byte hash, represented as a hex string.
     */
    std::string sha256(const std::string& data);

    /**
     * @brief Generates a digital signature for a piece of data.
     *
     * @param data The data to sign.
     * @param privateKey The private key to use for signing.
     * @return The generated signature as a string.
     */
    std::string sign(const std::string& data, const std::string& privateKey);

    /**
     * @brief Verifies a digital signature.
     *
     * @param data The original data.
     * @param signature The signature to verify.
     * @param publicKey The public key corresponding to the private key used for
     * signing.
     * @return true if the signature is valid, false otherwise.
     */
    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey);

  }  // namespace crypto
}  // namespace sbmpi

#endif  // SBMPI_CRYPTO_H
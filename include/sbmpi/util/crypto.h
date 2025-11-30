#ifndef SBMPI_CRYPTO_H
#define SBMPI_CRYPTO_H

#include <string>
#include <vector>
#include <array>
#include "../core/state/transaction.h"

namespace sbmpi {
namespace util {

const static size_t KEYLEN = 32;

std::string toHex(const std::vector<unsigned char>& data);
std::vector<unsigned char> generatePrivateKey();
std::vector<unsigned char> derivePublicKey(const std::vector<unsigned char>& privateKey);
std::vector<unsigned char> keccak256(const std::vector<unsigned char>& data);
std::string deriveAddress(const std::vector<unsigned char>& publicKey);
std::string recoverAddress(const std::vector<unsigned char>& signature, const std::vector<unsigned char>& hash);
std::string sha256(const std::string& data);
std::vector<unsigned char> sign(const std::vector<unsigned char>& hash, const std::vector<unsigned char>& privateKey);
bool verify(const std::string& data, const std::string& signature,
            const std::string& publicKey);
std::string merkle(const std::vector<core::state::Transaction>& transactions);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_CRYPTO_H

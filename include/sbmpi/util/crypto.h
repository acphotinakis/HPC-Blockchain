#ifndef SBMPI_CRYPTO_H
#define SBMPI_CRYPTO_H

#include <string>
#include <vector>
#include "../core/state/transaction.h"

namespace sbmpi {
namespace util {

std::string sha256(const std::string& data);
std::string sign(const std::string& data, const std::string& privateKey);
bool verify(const std::string& data, const std::string& signature,
            const std::string& publicKey);
std::string merkle(const std::vector<core::state::Transaction>& transactions);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_CRYPTO_H

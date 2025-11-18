#include "../../include/sbmpi/util/crypto.h"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

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

  }  // namespace util
}  // namespace sbmpi

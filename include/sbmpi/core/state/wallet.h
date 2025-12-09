#ifndef SPMPI_WALLET_H
#define SPMPI_WALLET_H

#include <array>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace sbmpi
{
  namespace core
  {
    namespace state
    {
      class Wallet
      {
       public:
        // A mock user's public key in a vector of hexadecimal bytes
        std::vector<unsigned char> publicKeyRaw;
        // A mock user's public key in a hexadecimal string
        std::string publicKeyHex;
        // A mock user's private key in a vector of hexadecimal bytes
        std::vector<unsigned char> privateKeyRaw;
        // A mock user's public key in a hexadecimal string
        std::string privateKeyHex;
        // A mock user's address in a hexadecimal string
        std::string address;
        uint64_t    nonce;
        /**
         * @brief Constructor for a new Wallet object.
         *
         * @param generateNew Specifies whether a Wallet's member variables
         * should be newly generated, or left empty.
         */
        Wallet(bool generateNew);

        /**
         * @brief Serializes wallet data to a `json` instance.
         *
         * Uses Niels Lohmann's C++ JSON library for straightforward JSON
         * parsing.
         * @return A `json` instance containing wallet data in JSON format.
         */
        json toJSON() const;

        /**
         * @brief Deserializes `json` data to populate a Transaction's data.
         *
         * Uses Niels Lohmann's C++ JSON library for straightforward JSON
         * reading.
         */
        void fromJSON(json& j);
      };
    }  // namespace state
  }  // namespace core
}  // namespace sbmpi
#endif  // SBMPI_WALLET_H
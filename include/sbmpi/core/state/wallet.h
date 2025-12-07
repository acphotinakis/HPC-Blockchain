#ifndef SPMPI_WALLET_H
#define SPMPI_WALLET_H

#include <string>
#include <array>
#include <vector>
#include <nlohmann/json.hpp>

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
                    std::vector<unsigned char> publicKeyRaw;
                    std::string publicKeyHex;
                    std::vector<unsigned char> privateKeyRaw;
                    std::string privateKeyHex;
                    std::string address;

                    Wallet(bool generateNew);
                    Wallet(
                        std::string _publicKey, std::string _privateKey, std::string _address
                    );

                    json toJSON() const;
                    void fromJSON(json& j);
            };
        }
    }
}
#endif  // SBMPI_WALLET_H
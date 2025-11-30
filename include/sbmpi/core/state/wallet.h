#ifndef SPMPI_WALLET_H
#define SPMPI_WALLET_H

#include <string>
#include <array>
#include <vector>

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

                    Wallet();
            };
        }
    }
}
#endif  // SBMPI_WALLET_H
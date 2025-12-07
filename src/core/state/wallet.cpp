/**
 * @file wallet.cpp
 * @brief Implements the Wallet class for representing user wallets (Ethereum style).
 */
#include "../../../include/sbmpi/core/state/wallet.h"
#include "../../../include/sbmpi/util/crypto.h"
#include <nlohmann/json.hpp>


namespace sbmpi
{
    namespace core
    {
        namespace state 
        {
            /**
             * @brief Constructs a new Wallet object.
             *
             * Generates a unique private key, public key, and address under
             * an Ethereum format.
             */
            Wallet::Wallet(bool generateNew) {
                if (generateNew) {
                    privateKeyRaw = util::generatePrivateKey(nullptr);
                    privateKeyHex = util::toHex(privateKeyRaw);

                    publicKeyRaw = util::derivePublicKey(privateKeyRaw);
                    publicKeyHex = util::toHex(publicKeyRaw);

                    address = util::deriveAddress(publicKeyRaw);
                } else {
                    privateKeyHex = "";
                    publicKeyHex = "";
                    address = "";
                }
                
            }

            Wallet::Wallet(
                std::string _publicKey, std::string _privateKey, std::string _address
            ) {
                privateKeyHex = _privateKey;
                privateKeyRaw = util::hexToBytes(_privateKey);

                publicKeyHex = _publicKey;
                publicKeyRaw = util::hexToBytes(_publicKey);

                address = _address;
            }

            json Wallet::toJSON() const {
                json walletJson = {
                    {"publicKey", publicKeyHex},
                    {"privateKey", privateKeyHex},
                    {"address", address}
                };
                return walletJson;
            }

            void Wallet::fromJSON(json& json) {
                publicKeyHex = json["publicKey"].get<std::string>();
                publicKeyRaw = util::hexToBytes(publicKeyHex);

                privateKeyHex = json["privateKey"].get<std::string>();
                privateKeyRaw = util::hexToBytes(privateKeyHex);

                address = json["address"].get<std::string>();
            }
        } // namespace state
    } // namespace core
} // namespace sbmpi
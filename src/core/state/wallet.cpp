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
             * Generates a unique private key, public key, and address in
             * an Ethereum format if specified; otherwise, leave empty.
             * @param generateNew Specifies whether a Wallet's member variables should 
             * be newly generated, or left empty.
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

            /**
             * @brief Serializes wallet data to a `json` instance.
             *
             * Uses Niels Lohmann's C++ JSON library for straightforward JSON parsing.
             * Fields to write: "publicKey", "privateKey", and "address".
             * @return A `json` instance containing wallet data in JSON format.
             */
            json Wallet::toJSON() const {
                json walletJson = {
                    {"publicKey", publicKeyHex},
                    {"privateKey", privateKeyHex},
                    {"address", address}
                };
                return walletJson;
            }

            /**
             * @brief Deserializes `json` data to populate a Transaction's data.
             *
             * Uses Niels Lohmann's C++ JSON library for straightforward JSON reading.
             * Fields to read: "publicKey", "privateKey", "address".
             */
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
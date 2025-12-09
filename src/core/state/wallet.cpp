/**
 * @file wallet.cpp
 * @brief Implements the Wallet class for representing user wallets (Ethereum
 * style).
 */
#include "../../../include/sbmpi/core/state/wallet.h"
#include <nlohmann/json.hpp>  // [Fix 1] Use consistent include style
#include "../../../include/sbmpi/util/crypto.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {
      /**
       * @brief Constructs a new Wallet object.
       *
       * Generates a unique private key, public key, and address.
       * @param generateNew If true, generates new keys.
       */
      Wallet::Wallet(bool generateNew)
      {
        // Initialize nonce to 0 for Replay Protection
        nonce = 0;

        if (generateNew) {
          // Create context locally to prevent memory leak in crypto.cpp
          secp256k1_context* ctx = secp256k1_context_create(
              SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN);

          try {
            // Pass the context so crypto.cpp doesn't create and leak one
            privateKeyRaw = util::generatePrivateKey(ctx);
            privateKeyHex = util::toHex(privateKeyRaw);

            publicKeyRaw = util::derivePublicKey(privateKeyRaw);
            publicKeyHex = util::toHex(publicKeyRaw);

            address = util::deriveAddress(publicKeyRaw);
          } catch (...) {
            secp256k1_context_destroy(ctx);
            throw;
          }

          secp256k1_context_destroy(ctx);  // Clean up context
        } else {
          privateKeyHex = "";
          publicKeyHex  = "";
          address       = "";
        }
      }

      /**
       * @brief Serializes wallet data to a `json` instance.
       */
      json Wallet::toJSON() const
      {
        json walletJson = {
            {"publicKey", publicKeyHex},
            {"privateKey", privateKeyHex},
            {"address", address},
            {"nonce", nonce}  // [Fix 3] Persist nonce
        };
        return walletJson;
      }

      /**
       * @brief Deserializes `json` data to populate a Wallet's data.
       */
      void Wallet::fromJSON(json& json)
      {
        publicKeyHex = json["publicKey"].get<std::string>();
        publicKeyRaw = util::hexToBytes(publicKeyHex);

        privateKeyHex = json["privateKey"].get<std::string>();
        privateKeyRaw = util::hexToBytes(privateKeyHex);

        address = json["address"].get<std::string>();

        // [Fix 3] Read nonce if available, default to 0
        if (json.contains("nonce")) {
          nonce = json["nonce"].get<uint64_t>();
        } else {
          nonce = 0;
        }
      }
    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

// /**
//  * @file wallet.cpp
//  * @brief Implements the Wallet class for representing user wallets (Ethereum
//  * style).
//  */
// #include "../../../include/sbmpi/core/state/wallet.h"
// #include <nlohmann/json.hpp>
// #include "../../../include/sbmpi/util/crypto.h"

// namespace sbmpi
// {
//   namespace core
//   {
//     namespace state
//     {
//       /**
//        * @brief Constructs a new Wallet object.
//        *
//        * Generates a unique private key, public key, and address in
//        * an Ethereum format if specified; otherwise, leave empty.
//        * @param generateNew Specifies whether a Wallet's member variables
//        should
//        * be newly generated, or left empty.
//        */
//       Wallet::Wallet(bool generateNew)
//       {
//         nonce = 0;
//         if (generateNew) {
//           privateKeyRaw = util::generatePrivateKey(nullptr);
//           privateKeyHex = util::toHex(privateKeyRaw);

//           publicKeyRaw = util::derivePublicKey(privateKeyRaw);
//           publicKeyHex = util::toHex(publicKeyRaw);

//           address = util::deriveAddress(publicKeyRaw);
//         } else {
//           privateKeyHex = "";
//           publicKeyHex  = "";
//           address       = "";
//         }
//       }

//       /**
//        * @brief Serializes wallet data to a `json` instance.
//        *
//        * Uses Niels Lohmann's C++ JSON library for straightforward JSON
//        parsing.
//        * Fields to write: "publicKey", "privateKey", and "address".
//        * @return A `json` instance containing wallet data in JSON format.
//        */
//       json Wallet::toJSON() const
//       {
//         json walletJson = {{"publicKey", publicKeyHex},
//                            {"privateKey", privateKeyHex},
//                            {"address", address}};
//         return walletJson;
//       }

//       /**
//        * @brief Deserializes `json` data to populate a Transaction's data.
//        *
//        * Uses Niels Lohmann's C++ JSON library for straightforward JSON
//        reading.
//        * Fields to read: "publicKey", "privateKey", "address".
//        */
//       void Wallet::fromJSON(json& json)
//       {
//         publicKeyHex = json["publicKey"].get<std::string>();
//         publicKeyRaw = util::hexToBytes(publicKeyHex);

//         privateKeyHex = json["privateKey"].get<std::string>();
//         privateKeyRaw = util::hexToBytes(privateKeyHex);

//         address = json["address"].get<std::string>();
//       }
//     }  // namespace state
//   }  // namespace core
// }  // namespace sbmpi
/**
 * @file wallet.cpp
 * @brief Implements the Wallet class for representing user wallets (Ethereum style).
 */
#include "../../../include/sbmpi/core/state/wallet.h"
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
             * Generates a unique private key, public key, and address under
             * an Ethereum format.
             */
            Wallet::Wallet() {
                privateKeyRaw = util::generatePrivateKey();
                privateKeyHex = util::toHex(privateKeyRaw);

                publicKeyRaw = util::derivePublicKey(privateKeyRaw);
                publicKeyHex = util::toHex(publicKeyRaw);

                address = util::deriveAddress(publicKeyRaw);
            }
        } // namespace state
    } // namespace core
} // namespace sbmpi
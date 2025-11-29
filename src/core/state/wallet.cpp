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
            Wallet::Wallet() {
                privateKeyRaw = util::generatePrivateKey();
                privateKeyHex = util::toHex(privateKeyRaw.data(), 32);

                publicKeyRaw = util::derivePublicKey(privateKeyRaw);
                publicKeyHex = util::toHex(publicKeyRaw.data(), publicKeyRaw.size());

                address = util::deriveAddress(publicKeyRaw);
            }
        } // namespace state
    } // namespace core
} // namespace sbmpi
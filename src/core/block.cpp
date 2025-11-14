// C++ Source File (Implementation logic)
#include "../../include/sbmpi/core/block.h"
#include <openssl/sha.h>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace sbmpi
{
  namespace core
  {
    /**
     * @brief Constructor for a new block.
     * @param block_num The block's height in the chain.
     * @param prev_hash The hash of the preceding block.
     * @param shard_id The ID of the shard that proposed this block.
     */
    Block::Block(uint64_t block_num, const std::string& prev_hash, int shard_id)
    {
      m_header_.block_number = block_num;
      m_header_.timestamp =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
      m_header_.previous_hash = prev_hash;
      m_header_.merkle_root   = "";
      m_header_.shard_id      = shard_id;
      m_hash_                 = calculateHash();
    }

    /**
     * @brief Adds a transaction to the block.
     * @param tx The transaction to add.
     */
    void Block::addTransaction(const Transaction& tx)
    {
      m_transactions_.push_back(tx);
    }

    /**
     * @brief Calculates the hash of the entire block (header + transactions).
     * @return The SHA-256 hash of the block.
     */
    string Block::calculateHash() const
    {
      std::ostringstream oss;
      oss << m_header_.block_number << m_header_.timestamp
          << m_header_.previous_hash << m_header_.merkle_root
          << m_header_.shard_id;

      // Add all transaction hashes
      for (const auto& tx : m_transactions_) oss << tx.getHash();

      const std::string str = oss.str();
      unsigned char     hash[SHA256_DIGEST_LENGTH];
      SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.size(),
             hash);

      std::ostringstream ss;
      for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

      return ss.str();
    }

    /**
     * @brief Gets the block's calculated hash.
     * @return The block hash.
     */
    std::string Block::getHash() const
    {
      return m_hash_;
    }

    /**
     * @brief Gets the block's header.
     * @return A constant reference to the block header.
     */
    const BlockHeader& Block::getHeader() const
    {
      return m_header_;
    }

    /**
     * @brief Gets the transactions in this block.
     * @return A constant reference to the transaction list.
     */
    const std::vector<Transaction>& Block::getTransactions() const
    {
      return m_transactions_;
    }

  }  // namespace core
}  // namespace sbmpi
/**
 * @file block.h
 * @brief Defines the Block and BlockHeader data structures.
 *
 * @headerfile block.h
 *
 * @details
 * The corresponding block.cpp will implement method logic, such as
 * hash calculation. A Block is a container for validated transactions
 *.
 *
 * In this project's architecture, two types of blocks are created:
 * 1.  **Micro-Block:** The result of a single Shard's successful PBFT
 * consensus.
 * 2.  **Final Block:** The block created by the Final Committee, which
 * assembles all micro-blocks.
 *
 * This class interacts with the `Transaction` class (which it contains)
 * and the `Blockchain` class (which contains it).
 */

#pragma once

#include "transaction.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace sbmpi {
namespace core {

/**
 * @struct BlockHeader
 * @brief Contains metadata for a block.
 */
struct BlockHeader {
  uint64_t block_number;
  uint64_t timestamp;
  std::string previous_hash;
  std::string merkle_root; // A hash representing all transactions
  int shard_id = -1;       // Which shard created this block
};

/**
 * @class Block
 * @brief Represents a single block in the blockchain.
 */
class Block {
public:
  /**
   * @brief Constructor for a new block.
   * @param block_num The block's height in the chain.
   * @param prev_hash The hash of the preceding block.
   * @param shard_id The ID of the shard that proposed this block.
   */
  Block(uint64_t block_num, const std::string &prev_hash, int shard_id);

  /**
   * @brief Adds a transaction to the block.
   * @param tx The transaction to add.
   */
  void addTransaction(const Transaction &tx);

  /**
   * @brief Calculates the hash of the entire block (header + transactions).
   * @return The SHA-256 hash of the block.
   */
  std::string calculateHash() const;

  /**
   * @brief Gets the block's calculated hash.
   * @return The block hash.
   */
  std::string getHash() const;

  /**
   * @brief Gets the block's header.
   * @return A constant reference to the block header.
   */
  const BlockHeader &getHeader() const;

  /**
   * @brief Gets the transactions in this block.
   * @return A constant reference to the transaction list.
   */
  const std::vector<Transaction> &getTransactions() const;

private:
  BlockHeader m_header_;
  std::vector<Transaction> m_transactions_;
  std::string m_hash_; // The calculated hash of this block
};

} // namespace core
} // namespace sbmpi
#ifndef SBMPI_CROSS_SHARD_H
#define SBMPI_CROSS_SHARD_H

#include "../core/state/transaction.h"

/**
 * @file cross_shard.h
 * @brief Defines logic for handling cross-shard transactions.
 *
 * This file provides the interface for functions and classes that manage
 * transactions spanning multiple shards. This is a complex part of a sharded
 * blockchain and requires a protocol to ensure atomicity. The implementation
 * in `src/network/cross_shard.cpp` would contain this logic.
 */

namespace sbmpi
{
  namespace cross_shard
  {

    /**
     * @brief Determines if a transaction is a cross-shard transaction.
     *
     * @param tx The transaction to check.
     * @return true if the transaction involves more than one shard, false
     * otherwise.
     */
    bool isCrossShard(const Transaction& tx);

    /**
     * @brief Coordinates the processing of a cross-shard transaction.
     *
     * This would involve a multi-phase commit protocol between the involved
     * shards.
     *
     * @param tx The cross-shard transaction to process.
     */
    void processTransaction(const Transaction& tx);

  }  // namespace cross_shard
}  // namespace sbmpi

#endif  // SBMPI_CROSS_SHARD_H
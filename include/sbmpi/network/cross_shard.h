#ifndef SBMPI_CROSS_SHARD_H
#define SBMPI_CROSS_SHARD_H

#include "../core/state/transaction.h"

namespace sbmpi
{
  namespace network
  {

    bool isCrossShard(const core::state::Transaction& tx);
    void processTransaction(const core::state::Transaction& tx);

  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_CROSS_SHARD_H

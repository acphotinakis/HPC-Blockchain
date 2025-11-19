#ifndef SBMPI_GENERATOR_H
#define SBMPI_GENERATOR_H

#include <vector>
#include "../core/state/transaction.h"

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Generates a deterministic set of mock transactions.
     * * This function uses a fixed random seed to ensure that the workload
     * is identical across different simulation runs, allowing for accurate
     * benchmarking of the sharding speedup.
     * * @param count The number of transactions to generate.
     * @return A vector of fully populated and signed Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count);

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_GENERATOR_H
#ifndef SBMPI_MEMPOOL_H
#define SBMPI_MEMPOOL_H

#include <mutex>
#include <vector>
#include "../state/transaction.h"

namespace sbmpi {
namespace core {
namespace mempool {

class Mempool {
 public:
  Mempool();
  bool add(const state::Transaction& tx);
  void remove(const std::string& transactionId);
  std::vector<state::Transaction> getTransactions(size_t maxCount);
  size_t size() const;

 private:
  mutable std::mutex mtx;
  std::vector<state::Transaction> transactions;
};

}  // namespace mempool
}  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MEMPOOL_H

#include "../../include/sbmpi/core/mempool/mempool.h"

#include <algorithm>
#include <vector>

namespace sbmpi {
namespace core {
namespace mempool {

Mempool::Mempool() {}

bool Mempool::add(const state::Transaction& tx) {
  std::lock_guard<std::mutex> lock(mtx);
  // Prevent duplicates
  auto it = std::find_if(transactions.begin(), transactions.end(),
                         [&](const state::Transaction& t) {
                           return t.id == tx.id;
                         });
  if (it == transactions.end()) {
    transactions.push_back(tx);
    return true;
  }
  return false;
}

void Mempool::remove(const std::string& transactionId) {
  std::lock_guard<std::mutex> lock(mtx);
  transactions.erase(
      std::remove_if(transactions.begin(), transactions.end(),
                     [&](const state::Transaction& tx) {
                       return tx.id == transactionId;
                     }),
      transactions.end());
}

std::vector<state::Transaction> Mempool::getTransactions(size_t maxCount) {
  std::lock_guard<std::mutex> lock(mtx);
  size_t count = std::min(maxCount, transactions.size());
  std::vector<state::Transaction> result(transactions.begin(),
                                         transactions.begin() + count);
  transactions.erase(transactions.begin(), transactions.begin() + count);
  return result;
}

size_t Mempool::size() const {
  std::lock_guard<std::mutex> lock(mtx);
  return transactions.size();
}

}  // namespace mempool
}  // namespace core
}  // namespace sbmpi

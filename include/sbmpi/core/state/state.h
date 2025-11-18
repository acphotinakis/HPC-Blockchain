#ifndef SBMPI_STATE_H
#define SBMPI_STATE_H

#include <map>
#include <string>
#include "transaction.h"

namespace sbmpi {
namespace core {
namespace state {

class State {
 public:
  State();
  bool apply(const Transaction& tx);
  double getBalance(const std::string& address) const;

 private:
  std::map<std::string, double> balances;
};

}  // namespace state
}  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_STATE_H

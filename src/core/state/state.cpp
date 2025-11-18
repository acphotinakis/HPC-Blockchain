#include "../../../include/sbmpi/core/state/state.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      State::State()
      {
        // Maybe some initial state, e.g. for genesis
        balances["genesis_address"] = 1000000.0;
      }

      bool State::apply(const Transaction& tx)
      {
        if (!tx.verify()) {
          return false;
        }

        auto from_it = balances.find(tx.from);
        if (from_it == balances.end() || from_it->second < tx.amount) {
          // Sender does not exist or has insufficient funds
          return false;
        }

        from_it->second -= tx.amount;

        auto to_it = balances.find(tx.to);
        if (to_it == balances.end()) {
          balances[tx.to] = tx.amount;
        } else {
          to_it->second += tx.amount;
        }

        return true;
      }

      double State::getBalance(const std::string& address) const
      {
        auto it = balances.find(address);
        if (it != balances.end()) {
          return it->second;
        }
        return 0.0;
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

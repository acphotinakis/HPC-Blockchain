#include "../../../include/sbmpi/core/state/transaction.h"

#include <chrono>
#include <sstream>
#include <vector>

#include "../../../include/sbmpi/util/crypto.h"
#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      Transaction::Transaction() : amount(0.0) {}

      Transaction::Transaction(const std::string& from, const std::string& to,
                               double amount)
          : from(from), to(to), amount(amount)
      {
        std::stringstream ss;
        ss << from << to << amount
           << std::chrono::system_clock::now().time_since_epoch().count();
        id = util::sha256(ss.str());
      }

      void Transaction::sign(const std::string& privateKey)
      {
        std::string data = from + to + std::to_string(amount);
        signature        = util::sign(data, privateKey);
      }

      bool Transaction::verify() const
      {
        std::string data = from + to + std::to_string(amount);
        // Assumes the 'from' address is the public key for dummy verification
        return util::verify(data, signature, from);
      }

      std::vector<char> Transaction::serialize() const
      {
        std::vector<char> buffer;
        util::pack(id, buffer);
        util::pack(from, buffer);
        util::pack(to, buffer);
        util::pack(amount, buffer);
        util::pack(signature, buffer);
        return buffer;
      }

      void Transaction::deserialize(const std::vector<char>& data)
      {
        int offset = 0;
        id         = util::unpack_string(data, offset);
        from       = util::unpack_string(data, offset);
        to         = util::unpack_string(data, offset);
        amount     = util::unpack_double(data, offset);
        signature  = util::unpack_string(data, offset);
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

#include "../../include/sbmpi/core/transaction.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
namespace sbmpi
{
  namespace core
  {

    std::vector<char> Transaction::serialize() const
    {
      std::vector<char> buffer(sizeof(Transaction));

      // Copy the memory of the struct into the vector
      std::memcpy(buffer.data(), this, sizeof(Transaction));

      return buffer;
    }

    void Transaction::deserialize(const std::vector<char>& buffer)
    {
      if (buffer.size() != sizeof(Transaction)) {
        throw std::runtime_error(
            "Buffer size does not match Transaction size!");
      }

      // Copy bytes back into this object
      std::memcpy(this, buffer.data(), sizeof(Transaction));
    }

    std::string Transaction::getHash() const
    {
      // Simple hash: hex representation of concatenated fields
      std::ostringstream oss;
      oss << std::hex << id << timestamp << sender_id << receiver_id
          << std::fixed << std::setprecision(2) << value;
      return oss.str();
    }

    std::vector<Transaction> createMockTransactions(size_t count)
    {
      std::vector<Transaction> transactions;
      transactions.reserve(count);

      std::random_device                      rd;
      std::mt19937                            gen(rd());
      std::uniform_int_distribution<uint32_t> userDist(1, 1000);
      std::uniform_real_distribution<double>  valueDist(0.01, 1000.0);

      for (size_t i = 0; i < count; ++i) {
        Transaction t;
        t.id          = i + 1;
        t.timestamp   = static_cast<uint64_t>(time(nullptr));
        t.sender_id   = userDist(gen);
        t.receiver_id = userDist(gen);
        t.value       = valueDist(gen);

        transactions.push_back(t);
      }

      return transactions;
    }

  }  // namespace core
}  // namespace sbmpi
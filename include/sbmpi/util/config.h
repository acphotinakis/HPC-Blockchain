#ifndef SBMPI_CONFIG_H
#define SBMPI_CONFIG_H

#include <string>

namespace sbmpi
{
  namespace util
  {

    class Config
    {
     public:
      int numNodes        = 0;
      int numShards       = 1;
      int numTransactions = 1000;
      int verbose         = 1;
      int runID           = 0;
      int seed            = 0;
      int transactionSize = 128;
      double faultProbability = 0.0;

      bool parse(int argc, char** argv);
      void print() const;
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_CONFIG_H

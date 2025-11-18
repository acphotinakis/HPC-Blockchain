#ifndef SBMPI_METRICS_H
#define SBMPI_METRICS_H

#include <string>

namespace sbmpi
{
  namespace util
  {

    class Metrics
    {
     public:
      static void   recordTime(const std::string& experimentName,
                               double totalTime, int numTransactions);
      static double calculateThroughput(double totalTime, int numTransactions);
      static void   save(const std::string& filepath);
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_METRICS_H

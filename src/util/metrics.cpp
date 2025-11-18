#include "../../include/sbmpi/util/metrics.h"
#include <fstream>
#include <iostream>
#include <map>

namespace sbmpi
{
  namespace util
  {

    namespace
    {
      struct ExperimentResult {
        std::string name;
        double      totalTime;
        int         numTransactions;
      };
      std::map<std::string, ExperimentResult> results;
    }  // namespace

    void Metrics::recordTime(const std::string& experimentName,
                             double totalTime, int numTransactions)
    {
      results[experimentName] = {experimentName, totalTime, numTransactions};
    }

    double Metrics::calculateThroughput(double totalTime, int numTransactions)
    {
      if (totalTime == 0) return 0;
      return numTransactions / totalTime;
    }

    void Metrics::save(const std::string& filepath)
    {
      std::ofstream file(filepath);
      if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
      }
      file << "Experiment,TotalTime,NumTransactions,Throughput" << std::endl;
      for (const auto& pair : results) {
        const auto& result = pair.second;
        double      throughput =
            calculateThroughput(result.totalTime, result.numTransactions);
        file << result.name << "," << result.totalTime << ","
             << result.numTransactions << "," << throughput << std::endl;
      }
    }

  }  // namespace util
}  // namespace sbmpi

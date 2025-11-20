/**
 * @file metrics.cpp
 * @brief Implements the Metrics class for recording and reporting simulation performance.
 */
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
      /**
       * @brief Structure to hold results for a single experiment.
       */
      struct ExperimentResult {
        std::string name;          ///< Name of the experiment.
        double      totalTime;     ///< Total time taken for the experiment.
        int         numTransactions; ///< Number of transactions processed.
      };
      std::map<std::string, ExperimentResult> results; ///< Stores all experiment results.
    }  // namespace

    /**
     * @brief The Metrics class provides static methods to record and save simulation performance metrics.
     *
     * It tracks total time and number of transactions for various experiments
     * and can calculate throughput, saving all data to a CSV file.
     */
    // No explicit constructor/destructor needed for a static utility class.

    /**
     * @brief Records the time and transaction count for a specific experiment.
     * @param experimentName A unique name for the experiment.
     * @param totalTime The total time measured for the experiment in seconds.
     * @param numTransactions The number of transactions processed during the experiment.
     */
    void Metrics::recordTime(const std::string& experimentName,
                             double totalTime, int numTransactions)
    {
      results[experimentName] = {experimentName, totalTime, numTransactions};
    }

    /**
     * @brief Calculates the throughput (transactions per second) for an experiment.
     * @param totalTime The total time taken for the experiment.
     * @param numTransactions The number of transactions processed.
     * @return The calculated throughput, or 0 if totalTime is zero.
     */
    double Metrics::calculateThroughput(double totalTime, int numTransactions)
    {
      if (totalTime == 0) return 0;
      return numTransactions / totalTime;
    }

    /**
     * @brief Saves all recorded metrics to a CSV file.
     *
     * The CSV file will contain columns for Experiment Name, Total Time,
     * Number of Transactions, and Calculated Throughput.
     * @param filepath The path to the output CSV file.
     */
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

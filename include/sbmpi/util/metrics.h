#ifndef SBMPI_METRICS_H
#define SBMPI_METRICS_H

#include <string>

/**
 * @file metrics.h
 * @brief Defines a utility for collecting and reporting performance metrics.
 *
 * This file provides the interface for the Metrics class, implemented in
 * `src/util/metrics.cpp`. This class is used to gather key performance
 * indicators during the simulation, such as throughput and latency, and
 * format them for output.
 */

class Metrics
{
 public:
  /**
   * @brief Records the total time taken for a specific operation.
   *
   * @param experimentName The name of the experiment (e.g., "serial",
   * "sharded_8").
   * @param totalTime The total duration in seconds.
   * @param numTransactions The number of transactions processed.
   */
  static void recordTime(const std::string& experimentName, double totalTime,
                         int numTransactions);

  /**
   * @brief Calculates and returns the throughput.
   *
   * @param totalTime The total duration in seconds.
   * @param numTransactions The number of transactions processed.
   * @return The throughput in transactions per second (TPS).
   */
  static double calculateThroughput(double totalTime, int numTransactions);

  /**
   * @brief Saves the collected metrics to a file.
   *
   * @param filepath The path to the output CSV file.
   */
  static void save(const std::string& filepath);
};

#endif  // SBMPI_METRICS_H
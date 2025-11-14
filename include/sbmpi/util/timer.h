/**
 * @file timer.h
 * @brief A utility for high-resolution performance measurement.
 *
 * @headerfile timer.h
 *
 * @details
 * The corresponding timer.cpp will implement the timing logic using
 * `std::chrono::high_resolution_clock`.
 *
 * This class is essential for fulfilling the "Performance Measurement"
 * section of the proposal. The root process (global rank 0)
 * in `main.cpp` will use this timer to measure the total execution time
 * for both the Baseline and Parallel models. The resulting times
 * (`Time_Serial` and `Time_Parallel`) are the raw data used to calculate
 * Throughput, Latency, and Speedup.
 */

#pragma once

#include <chrono>

namespace sbmpi
{
  namespace util
  {

    /**
     * @class Timer
     * @brief A simple high-resolution timer for benchmarking.
     */
    class Timer
    {
     public:
      /**
       * @brief Starts the timer.
       */
      void start();

      /**
       * @brief Stops the timer.
       */
      void stop();

      /**
       * @brief Gets the elapsed duration in seconds.
       * @return The duration in seconds as a double.
       */
      double getDurationSeconds() const;

      /**
       * @brief Gets the elapsed duration in milliseconds.
       * @return The duration in milliseconds as a double.
       */
      double getDurationMilliseconds() const;

     private:
      std::chrono::high_resolution_clock::time_point m_start_time_;
      std::chrono::high_resolution_clock::time_point m_end_time_;
    };

  }  // namespace util
}  // namespace sbmpi
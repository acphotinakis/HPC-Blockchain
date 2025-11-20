/**
 * @file timer.cpp
 * @brief Implements the Timer class for measuring elapsed time with high precision.
 */
#include "../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief A utility class for measuring elapsed time with high resolution.
     *
     * Provides methods to start, stop, and retrieve the elapsed time in
     * seconds or milliseconds.
     */
    // Default constructor is implicitly defined and sufficient.

    /**
     * @brief Starts the timer, recording the current high-resolution time point.
     */
    void Timer::start()
    {
      startTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Stops the timer, recording the current high-resolution time point.
     */
    void Timer::stop()
    {
      endTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Calculates the elapsed time between start() and stop() in seconds.
     * @return The elapsed time in double-precision seconds.
     */
    double Timer::elapsedSeconds() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double>>(
                 endTime - startTime)
          .count();
    }

    /**
     * @brief Calculates the elapsed time between start() and stop() in milliseconds.
     * @return The elapsed time in double-precision milliseconds.
     */
    double Timer::elapsedMilliseconds() const
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                   startTime)
          .count();
    }

  } // namespace util
} // namespace sbmpi

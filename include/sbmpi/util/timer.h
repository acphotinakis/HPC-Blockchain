#ifndef SBMPI_TIMER_H
#define SBMPI_TIMER_H

#include <chrono>

/**
 * @file timer.h
 * @brief Defines a high-resolution timer for performance measurements.
 *
 * The Timer class provides a simple way to measure elapsed time, which is
 * crucial for evaluating the performance of the serial vs. parallel models.
 */

class Timer {
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
     * @brief Gets the elapsed time in seconds.
     *
     * @return The elapsed time as a double.
     */
    double elapsedSeconds() const;

    /**
     * @brief Gets the elapsed time in milliseconds.
     *
     * @return The elapsed time as a double.
     */
    double elapsedMilliseconds() const;

private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
};

#endif // SBMPI_TIMER_H
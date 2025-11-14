/**
 * @file logging.h
 * @brief Provides a simple, MPI-aware logging utility.
 *
 * @headerfile logging.h
 *
 * @details
 * The corresponding logging.cpp will implement the logic for formatting
 * log messages. Standard `printf` or `std::cout` is unusable in a large-scale
 * MPI application, as output from all processes will be interleaved and
 * unreadable.
 *
 * This utility prefixes each log message with the node's global MPI rank,
 * e.g., `[Rank 0] [INFO] Simulation started.` This is critical for
 * debugging parallel logic, especially when tracking the state of PBFT
 * (pre-prepare, prepare, commit) across different nodes.
 *
 * It will be instantiated once in `main.cpp` and passed by reference
 * to other components (Node, Shard, PBFT).
 */

#pragma once

#include <string>
#include <sstream>

namespace sbmpi {
namespace util {

/**
 * @enum LogLevel
 * @brief Defines the verbosity level for the logger.
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

/**
 * @class Logger
 * @brief A simple MPI-aware logging class.
 */
class Logger {
public:
    /**
     * @brief Constructor.
     * @param rank The global MPI rank of the process using this logger.
     */
    Logger(int rank, LogLevel level = LogLevel::INFO)
        : mpi_rank_(rank), m_level_(level) {}

    /**
     * @brief Logs a message at the INFO level.
     * @param message The message to log.
     */
    void info(const std::string& message) const;

    /**
     * @brief Logs a message at the DEBUG level.
     * @param message The message to log.
     */
    void debug(const std::string& message) const;

    /**
     * @brief Logs a message at the ERROR level.
     * @param message The message to log.
     */
    void error(const std::string& message) const;

private:
    /**
     * @brief Internal logging function.
     * @param level The log level.
     * @param message The message.
     */
    void log(LogLevel level, const std::string& message) const;

    int mpi_rank_;
    LogLevel m_level_;
};

} // namespace util
} // namespace sbmpi
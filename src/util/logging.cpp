/**
 * @file logging.cpp
 * @brief Implements the Logger class for structured and level-based logging.
 */
#include "../../include/sbmpi/util/logging.h"
#include <iostream>
#include <mutex>
#include "sbmpi/util/errors.h"

namespace sbmpi
{
  namespace util
  {

    // Definition of static member variable
    LogLevel Logger::loggerLevel = LogLevel::INFO;

    // Mutex to prevent garbled output if multiple threads log simultaneously
    // (Note: MPI processes are separate, but threads within a process need
    // this)
    static std::mutex log_mutex;

    /**
     * @brief Configures the logger with the MPI rank of the current process.
     *
     * This rank is prepended to log messages to identify the source.
     * @param r The MPI rank of the process.
     */
    void Logger::configure(int r)
    {
      this->rank = r;
    }

    /**
     * @brief Sets the minimum logging level.
     *
     * Messages with a level equal to or higher than the set level will be logged.
     * @param level The new minimum LogLevel.
     */
    void Logger::setLevel(LogLevel level)
    {
      loggerLevel = level;
    }

    /**
     * @brief Logs a message with a specified level.
     *
     * Messages are printed to standard output, prefixed with the MPI rank and
     * the log level string. Thread-safe due to a mutex.
     * @param level The LogLevel of the message.
     * @param message The string content of the log message.
     */
    void Logger::log(LogLevel level, const std::string& message)
    {
      if (level <= loggerLevel) {
        std::lock_guard<std::mutex> lock(log_mutex);

        std::string levelStr;
        switch (level) {
          case LogLevel::ERROR:
            levelStr = "[ERROR]";
            break;
          case LogLevel::INFO:
            levelStr = "[INFO] ";
            break;
          case LogLevel::DEBUG:
            levelStr = "[DEBUG]";
            break;
          case LogLevel::FATAL:
            levelStr = "[FATAL]";
            break;
          default:
            return;
        }

        // Format: [Rank 0] [INFO] Message content
        std::cout << "[Rank " << rank << "] " << levelStr << " " << message
                  << std::endl;
      }
    }

    /**
     * @brief Logs an informational message.
     * @param message The string content of the informational message.
     */
    void Logger::info(const std::string& message)
    {
      log(LogLevel::INFO, message);
    }

    /**
     * @brief Logs an error message.
     * @param message The string content of the error message.
     */
    void Logger::error(const std::string& message)
    {
      log(LogLevel::ERROR, message);
    }

    /**
     * @brief Logs a debug message.
     * @param message The string content of the debug message.
     */
    void Logger::debug(const std::string& message)
    {
      log(LogLevel::DEBUG, message);
    }

    /**
     * @brief Logs a fatal error message and terminates the program.
     *
     * This function also calls the global `sbmpi::util::fatal` function to
     * ensure program termination with the specified error code.
     * @param code The ErrorCode associated with the fatal error.
     * @param message The string content of the fatal error message.
     */
    void Logger::fatal(ErrorCode code, const std::string& message)
    {
      log(LogLevel::FATAL, message);
      // CRITICAL: Also call the global fatal function to terminate the program.
      // The global fatal function is responsible for printing to stderr and exiting.
      sbmpi::util::fatal(code, message);
    }

  } // namespace util
} // namespace sbmpi

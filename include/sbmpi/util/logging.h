#ifndef SBMPI_LOGGING_H
#define SBMPI_LOGGING_H

#include <string>
#include <sstream>

/**
 * @file logging.h
 * @brief Defines a simple logging utility for the simulation.
 *
 * This provides a basic, thread-safe logging mechanism to print formatted
 * messages to the console, prefixed with node-specific information.
 */

// Defines different levels of logging verbosity.
enum class LogLevel {
    NONE,
    ERROR,
    INFO,
    DEBUG
};

class Logger {
public:
    /**
     * @brief Sets the logging level for the application.
     *
     * @param level The maximum log level to display.
     */
    static void setLevel(LogLevel level);

    /**
     * @brief Logs a message.
     *
     * @param level The level of the message.
     * @param message The message to log.
     */
    static void log(LogLevel level, const std::string& message);

private:
    static LogLevel currentLevel;
};

// A helper macro for easy logging
#define LOG(level, message) \
    do { \
        std::stringstream ss;
        ss << message;
        Logger::log(level, ss.str()); \
    } while (0)

#endif // SBMPI_LOGGING_H
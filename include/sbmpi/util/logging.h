#ifndef SBMPI_LOGGING_H
#define SBMPI_LOGGING_H

#include <string>
#include <cstdio> // Required for snprintf
#include "sbmpi/util/errors.h"

namespace sbmpi
{
  namespace util
  {

    enum class LogLevel {
      NONE  = 0,
      ERROR = 1,
      INFO  = 2,
      DEBUG = 3,
      FATAL = 4
    };

    class Logger
    {
     private:
      // Private constructor for Singleton pattern
      Logger() = default;

      // Static log level (shared across the process)
      static LogLevel loggerLevel;

      // The MPI rank of this process (defaults to -1 until configured)
      int rank = -1;

     public:
      // Delete copy constructors
      Logger(const Logger&)         = delete;
      void operator=(const Logger&) = delete;

      /**
       * @brief Access the Singleton Logger instance.
       */
      static Logger& getLogger()
      {
        static Logger instance;
        return instance;
      }

      /**
       * @brief Configure the logger with the MPI rank.
       * Should be called immediately after MPI_Init.
       */
      void configure(int rank);

      /**
       * @brief Set the verbosity level.
       */
      void setLevel(LogLevel level);

      /**
       * @brief Log a message if the level permits.
       */
      void log(LogLevel level, const std::string& message);

      // Convenience helpers
      void info(const std::string& message);
      void error(const std::string& message);
      void debug(const std::string& message);

      // Non-variadic fatal method (existing)
      void fatal(ErrorCode errorCode, const std::string& message);

      // Variadic template fatal method for printf-style formatting
      template <typename... Args>
      void fatal(ErrorCode errorCode, const char* format, Args... args) {
          // Use a fixed-size buffer to format the message.
          // A more robust solution for production might involve dynamic allocation
          // or C++20's std::format.
          char buffer[1024]; // Max 1023 characters + null terminator
          snprintf(buffer, sizeof(buffer), format, args...);
          this->fatal(errorCode, std::string(buffer)); // Call the non-variadic fatal
      }
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_LOGGING_H
// #ifndef SBMPI_LOGGING_H
// #define SBMPI_LOGGING_H

// #include <iostream>
// #include <sstream>
// #include <string>

// namespace sbmpi
// {
//   namespace util
//   {

//     enum class LogLevel {
//       NONE,
//       ERROR,
//       INFO,
//       DEBUG
//     };

//     class Logger
//     {
//      private:
//       Logger() = default;
//       static LogLevel loggerLevel;

//      public:
//       static Logger& getLogger()
//       {
//         static Logger instance;
//         return instance;
//       }

//       void setLevel(LogLevel level)
//       {
//         loggerLevel = level;
//       }

//       void log(LogLevel level, const std::string& message)
//       {
//         if (level <= loggerLevel) {
//           std::cout << message << std::endl;
//         }
//       }
//     };
//   }  // namespace util
// }  // namespace sbmpi

// #endif  // SBMPI_LOGGING_H

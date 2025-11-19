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

    void Logger::configure(int r)
    {
      this->rank = r;
    }

    void Logger::setLevel(LogLevel level)
    {
      loggerLevel = level;
    }

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

    void Logger::info(const std::string& message)
    {
      log(LogLevel::INFO, message);
    }

    void Logger::error(const std::string& message)
    {
      log(LogLevel::ERROR, message);
    }

    void Logger::debug(const std::string& message)
    {
      log(LogLevel::DEBUG, message);
    }

    void Logger::fatal(ErrorCode code, const std::string& message)
    {
      log(LogLevel::FATAL, message);
      // CRITICAL: Also call the global fatal function to terminate the program.
      // The global fatal function is responsible for printing to stderr and exiting.
      sbmpi::util::fatal(code, message);
    }

  }  // namespace util
}  // namespace sbmpi
// #include "../../include/sbmpi/util/logging.h"
// #include <iostream>

// namespace sbmpi
// {
//   namespace util
//   {

//     LogLevel Logger::loggerLevel = LogLevel::INFO;

//   }  // namespace util
// }  // namespace sbmpi

#include "../../include/sbmpi/util/logging.h"
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    void Logger::log(LogLevel level, const std::string& message) const
    {
      if (level < m_level_) {
        return;
      }

      std::string level_str;
      switch (level) {
        case LogLevel::DEBUG:
          level_str = "DEBUG";
          break;
        case LogLevel::INFO:
          level_str = "INFO";
          break;
        case LogLevel::WARN:
          level_str = "WARN";
          break;
        case LogLevel::ERROR:
          level_str = "ERROR";
          break;
      }

      // Using std::cout for simplicity. A more robust logger might use std::cerr for errors.
      std::cout << "[Rank " << mpi_rank_ << "] [" << level_str << "] " << message << std::endl;
    }

    void Logger::info(const std::string& message) const
    {
      log(LogLevel::INFO, message);
    }

    void Logger::debug(const std::string& message) const
    {
      log(LogLevel::DEBUG, message);
    }

    void Logger::error(const std::string& message) const
    {
      log(LogLevel::ERROR, message);
    }

  }  // namespace util
}  // namespace sbmpi
#include "../../include/sbmpi/util/logging.h"
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    LogLevel Logger::currentLevel = LogLevel::INFO;

    void Logger::setLevel(LogLevel level)
    {
      currentLevel = level;
    }

    void Logger::log(LogLevel level, const std::string& message)
    {
      if (level <= currentLevel) {
        std::cout << message << std::endl;
      }
    }

  }  // namespace util
}  // namespace sbmpi

#ifndef SBMPI_LOGGING_H
#define SBMPI_LOGGING_H

#include <sstream>
#include <iostream>
#include <string>

namespace sbmpi
{
  namespace util
  {

    enum class LogLevel {
      NONE,
      ERROR,
      INFO,
      DEBUG
    };

    class Logger
    {
      private:
        Logger() = default;
        static LogLevel loggerLevel;

     public:
      static Logger& getLogger() 
      {
        static Logger instance;
        return instance;
      }

      void setLevel(LogLevel level)
      {
          loggerLevel = level;
      }

      void log(LogLevel level, const std::string& message)
      {
        if (level <= loggerLevel) 
        {
            std::cout << message << std::endl;
        }
    }
    };
}  // namespace sbmpi
}  // namespace sbmpi

#endif  // SBMPI_LOGGING_H

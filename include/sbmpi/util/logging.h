#ifndef SBMPI_LOGGING_H
#define SBMPI_LOGGING_H

#include <sstream>
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
     public:
      static void setLevel(LogLevel level);
      static void log(LogLevel level, const std::string& message);

     private:
      static LogLevel currentLevel;
    };

#define LOG(level, message) \
  do {                      \
    std::stringstream ss;   \
    ss << message;
    Logger::log(level, ss.str());
  }  // namespace util
  while (0) }  // namespace sbmpi
}  // namespace sbmpi

#endif  // SBMPI_LOGGING_H

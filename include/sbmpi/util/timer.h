#ifndef SBMPI_TIMER_H
#define SBMPI_TIMER_H

#include <chrono>

namespace sbmpi {
namespace util {

class Timer
{
 public:
  void start();
  void stop();
  double elapsedSeconds() const;
  double elapsedMilliseconds() const;

 private:
  std::chrono::high_resolution_clock::time_point startTime;
  std::chrono::high_resolution_clock::time_point endTime;
};

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_TIMER_H

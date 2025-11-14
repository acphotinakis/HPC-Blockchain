#include "../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace util
  {

    void Timer::start()
    {
      m_start_time_ = std::chrono::high_resolution_clock::now();
    }

    void Timer::stop()
    {
      m_end_time_ = std::chrono::high_resolution_clock::now();
    }

    double Timer::getDurationSeconds() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double>>(
                 m_end_time_ - m_start_time_)
          .count();
    }

    double Timer::getDurationMilliseconds() const
    {
      return std::chrono::duration_cast<
                 std::chrono::duration<double, std::milli>>(
                 m_end_time_ - m_start_time_)
          .count();
    }

  }  // namespace util
}  // namespace sbmpi
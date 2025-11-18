#include "../../include/sbmpi/util/timer.h"

namespace sbmpi {
namespace util {

void Timer::start() {
    startTime = std::chrono::high_resolution_clock::now();
}

void Timer::stop() {
    endTime = std::chrono::high_resolution_clock::now();
}

double Timer::elapsedSeconds() const {
    return std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime).count();
}

double Timer::elapsedMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

}  // namespace util
}  // namespace sbmpi

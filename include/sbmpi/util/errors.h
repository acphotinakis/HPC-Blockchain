#ifndef SBMPI_ERRORS_H
#define SBMPI_ERRORS_H

#include <string>

namespace sbmpi {
namespace util {

enum class ErrorCode {
  SUCCESS = 0,
  MPI_INIT_FAILED,
  INVALID_ARGUMENTS,
};

void fatal(ErrorCode code, const std::string& message);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_ERRORS_H

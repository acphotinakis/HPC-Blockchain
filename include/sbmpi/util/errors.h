#ifndef SBMPI_ERRORS_H
#define SBMPI_ERRORS_H

#include <string>

/**
 * @file errors.h
 * @brief Defines custom error handling utilities and error codes.
 *
 * This file provides a centralized place for defining error codes and functions
 * for handling fatal errors in the simulation. The implementation in
 * `src/util/errors.cpp` will typically log an error message and terminate
 * the MPI environment gracefully.
 */

namespace sbmpi
{
  namespace errors
  {

    // Enum for different error codes
    enum class ErrorCode {
      SUCCESS = 0,
      MPI_INIT_FAILED,
      INVALID_ARGUMENTS,
      // Add other specific error codes here
    };

    /**
     * @brief Handles a fatal error by printing a message and aborting.
     *
     * @param code The error code.
     * @param message A descriptive error message.
     */
    void fatal(ErrorCode code, const std::string& message);

  }  // namespace errors
}  // namespace sbmpi

#endif  // SBMPI_ERRORS_H
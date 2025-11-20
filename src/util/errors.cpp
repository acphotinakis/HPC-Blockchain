/**
 * @file errors.cpp
 * @brief Implements utility functions for error handling and program termination.
 */
#include "../../include/sbmpi/util/errors.h"
#include <cstdlib>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Logs a fatal error message to stderr and terminates the program.
     * @param code The ErrorCode representing the type of error.
     * @param message A descriptive error message.
     */
    void fatal(ErrorCode code, const std::string& message)
    {
      std::cerr << "Fatal Error [" << static_cast<int>(code) << "]: " << message
                << std::endl;
      exit(static_cast<int>(code));
    }

  } // namespace util
} // namespace sbmpi

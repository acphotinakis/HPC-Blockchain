#include "../../include/sbmpi/util/errors.h"
#include <cstdlib>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    void fatal(ErrorCode code, const std::string& message)
    {
      std::cerr << "Fatal Error [" << static_cast<int>(code) << "]: " << message
                << std::endl;
      exit(static_cast<int>(code));
    }

  }  // namespace util
}  // namespace sbmpi

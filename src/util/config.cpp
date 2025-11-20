/**
 * @file config.cpp
 * @brief Implements the Config class for parsing command-line arguments and managing simulation settings.
 */
#include "../../include/sbmpi/util/config.h"
#include <iostream>
#include <string>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Manages configuration settings for the simulation, typically parsed from command-line arguments.
     *
     * This class provides methods to parse arguments and print the current configuration.
     */
    // Assuming the definition of Config::parse is in config.cpp
    // and the declarations of numNodes, numShards, numTransactions, verbose
    // are in config.h

    /**
     * @brief Parses command-line arguments to configure simulation parameters.
     *
     * Supports short and long flags for number of shards (-s, --shards) and
     * number of transactions (-t, --transactions). Also includes placeholders
     * for numNodes and verbose settings.
     * @param argc The number of command-line arguments.
     * @param argv An array of command-line argument strings.
     * @return True if arguments were parsed successfully, false otherwise.
     */
    bool Config::parse(int argc, char** argv)
    {
      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // ---  Handle Long Flags from Makefile ---
        if ((arg == "-s" || arg == "--shards") && i + 1 < argc) {
          // Argument is the number of shards
          numShards = std::stoi(argv[++i]);

        } else if ((arg == "-t" || arg == "--transactions") && i + 1 < argc) {
          // Argument is the number of transactions
          numTransactions = std::stoi(argv[++i]);

          // Keep existing rules for completeness, though unused by the current
          // Makefile ARGS
        } else if (arg == "-n" && i + 1 < argc) {
          numNodes = std::stoi(argv[++i]);
        } else if (arg == "-v" && i + 1 < argc) {
          verbose = std::stoi(argv[++i]);

        } else {
          // If the argument is none of the above, it is unknown
          std::cerr << "Unknown argument: " << arg << std::endl;
          return false;
        }
      }
      return true;
    }

    /**
     * @brief Prints the current configuration settings to standard output.
     */
    void Config::print() const
    {
      std::cout << "Configuration:" << std::endl;
      std::cout << "  Nodes: " << numNodes << std::endl;
      std::cout << "  Shards: " << numShards << std::endl;
      std::cout << "  Transactions: " << numTransactions << std::endl;
      std::cout << "  Verbose: " << verbose << std::endl;
    }

  } // namespace util
} // namespace sbmpi
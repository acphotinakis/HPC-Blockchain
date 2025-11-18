#include "../../include/sbmpi/util/config.h"
#include <iostream>
#include <string>

namespace sbmpi
{
  namespace util
  {

    bool Config::parse(int argc, char** argv)
    {
      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc) {
          numNodes = std::stoi(argv[++i]);
        } else if (arg == "-s" && i + 1 < argc) {
          numShards = std::stoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
          numTransactions = std::stoi(argv[++i]);
        } else if (arg == "-v" && i + 1 < argc) {
          verbose = std::stoi(argv[++i]);
        } else {
          std::cerr << "Unknown argument: " << arg << std::endl;
          return false;
        }
      }
      return true;
    }

    void Config::print() const
    {
      std::cout << "Configuration:" << std::endl;
      std::cout << "  Nodes: " << numNodes << std::endl;
      std::cout << "  Shards: " << numShards << std::endl;
      std::cout << "  Transactions: " << numTransactions << std::endl;
      std::cout << "  Verbose: " << verbose << std::endl;
    }

  }  // namespace util
}  // namespace sbmpi

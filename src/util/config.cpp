#include "../../include/sbmpi/util/config.h"
#include <iostream>
#include <string>
#include <vector>

namespace sbmpi
{
  namespace util
  {

    ExperimentConfig loadConfig(int argc, char* argv[])
    {
      ExperimentConfig config;
      std::vector<std::string> args(argv + 1, argv + argc);

      for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--shards" && i + 1 < args.size()) {
          try {
            config.num_shards = std::stoi(args[++i]);
          } catch (const std::exception& e) {
            if (i > 0) { // Check if argv[0] is available
                std::cerr << "Invalid argument for --shards: " << args[i] << std::endl;
            }
            exit(1);
          }
        } else if (args[i] == "--transactions" && i + 1 < args.size()) {
          try {
            config.total_transactions = std::stoul(args[++i]);
          } catch (const std::exception& e) {
            if (i > 0) {
                std::cerr << "Invalid argument for --transactions: " << args[i] << std::endl;
            }
            exit(1);
          }
        } else if (args[i] == "--faulty-nodes" && i + 1 < args.size()) {
          try {
            config.faulty_nodes_per_shard = std::stoi(args[++i]);
          } catch (const std::exception& e) {
            if (i > 0) {
                std::cerr << "Invalid argument for --faulty-nodes: " << args[i] << std::endl;
            }
            exit(1);
          }
        } else if (args[i] == "--final-committee-size" && i + 1 < args.size()) {
          try {
            config.final_committee_size = std::stoi(args[++i]);
          } catch (const std::exception& e) {
            if (i > 0) {
                std::cerr << "Invalid argument for --final-committee-size: " << args[i] << std::endl;
            }
            exit(1);
          }
        } else if (args[i] == "--log-level" && i + 1 < args.size()) {
          try {
            config.log_level = std::stoi(args[++i]);
          } catch (const std::exception& e) {
            if (i > 0) {
                std::cerr << "Invalid argument for --log-level: " << args[i] << std::endl;
            }
            exit(1);
          }
        }
      }
      return config;
    }

  }  // namespace util
}  // namespace sbmpi

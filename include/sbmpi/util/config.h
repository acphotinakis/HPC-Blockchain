/**
 * @file config.h
 * @brief Defines configuration structures and constants for the simulation.
 *
 * @headerfile config.h
 *
 * @details
 * The corresponding config.cpp file would be responsible for parsing command-line
 * arguments (e.g., using flags like `--shards 8 --transactions 10000`) 
 * or reading a configuration file. This module
 * centralizes all experimental parameters, making it easy to run different
 * simulation scenarios as required by the `run_experiment.sh` script
 *.
 *
 * This struct will be populated by the root process (global rank 0) and then
 * broadcast to all other processes using `MPI_Bcast` at the beginning of
 * `main.cpp` to ensure all nodes operate with the same parameters.
 */

#pragma once

#include <cstddef>

namespace sbmpi {
namespace util {

/**
 * @struct ExperimentConfig
 * @brief Holds all configurable parameters for a simulation run.
 */
struct ExperimentConfig {
    /**
     * @brief Total number of transactions to generate and process.
     */
    size_t total_transactions = 10000;

    /**
     * @brief The number of shards (committees) to partition the network into.
     * This is the primary independent variable for our experiments
     *. A value of 1 implies the Baseline (Serial) Model
     *.
     */
    int num_shards = 8;

    /**
     * @brief The number of faulty nodes (f) the PBFT protocol should tolerate.
     * The total number of nodes in a shard must be N >= 3f + 1
     *.
     */
    int faulty_nodes_per_shard = 1;

    /**
     * @brief The number of nodes to assign to the final committee.
     */
    int final_committee_size = 4;

    /**
     * @brief Verbosity level for the logger.
     */
    int log_level = 1; // 0=Error, 1=Info, 2=Debug
};

/**
 * @brief Parses command-line arguments to populate the ExperimentConfig.
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @return A populated ExperimentConfig struct.
 */
ExperimentConfig loadConfig(int argc, char* argv[]);

} // namespace util
} // namespace sbmpi
#ifndef SBMPI_CONFIG_H
#define SBMPI_CONFIG_H

#include <string>

/**
 * @file config.h
 * @brief Defines a configuration loading and management utility.
 *
 * The Config class is responsible for parsing command-line arguments or reading
 * a configuration file to set up the parameters for the blockchain simulation,
 * such as the number of shards, number of transactions, etc.
 */

class Config {
public:
    // The total number of nodes in the simulation.
    int numNodes = 0;
    // The number of shards to partition the network into.
    int numShards = 1;
    // The total number of transactions to generate and process.
    int numTransactions = 1000;
    // Verbosity level for logging.
    int verbose = 1;

    /**
     * @brief Parses command-line arguments to populate configuration settings.
     *
     * @param argc The argument count.
     * @param argv The argument vector.
     * @return true if parsing was successful, false otherwise.
     */
    bool parse(int argc, char** argv);

    /**
     * @brief Prints the current configuration settings.
     */
    void print() const;
};

#endif // SBMPI_CONFIG_H
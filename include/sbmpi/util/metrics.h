#ifndef SBMPI_METRICS_H
#define SBMPI_METRICS_H

#include <string>
#include <vector>

namespace sbmpi {
namespace util {

class SimulationMetrics {
public:
    static void record(const std::string& experimentName, int runID, int np, int numShards, int numTransactions, double totalTime);
    static void save(const std::string& filepath);
};

class ConsensusMetrics {
public:
    static void record(const std::string& experimentName, int runID, int shardID, const std::string& role, int pbftRounds, double consensusTime, int messagesExchanged, int failedAttempts);
    static void save(const std::string& filepath);
};

class BlockMetrics {
public:
    static void record(const std::string& experimentName, int runID, const std::string& blockID, const std::string& blockType, int numTransactions, double blockCreationTime, const std::string& prevBlockHash);
    static void save(const std::string& filepath);
};

class NodeMetrics {
public:
    static void record(const std::string& experimentName, int runID, int finalCommitteeSize, int microBlocksGenerated, int macroBlocksGenerated);
    static void save(const std::string& filepath);
};

class ShardMetrics {
public:
    static void record(const std::string& experimentName, int runID, int shardID, int numTransactions);
    static void save(const std::string& filepath);
};

class ExperimentParameters {
public:
    static void record(const std::string& experimentName, int runID, int np, int numShards, int numTransactions, int transactionSize, int seed);
    static void save(const std::string& filepath);
};

} // namespace util
} // namespace sbmpi

#endif // SBMPI_METRICS_H
#include "../../include/sbmpi/util/metrics.h"
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>

namespace sbmpi {
namespace util {

namespace {
    struct SimulationResult {
        std::string experimentName;
        int runID;
        int np;
        int numShards;
        int numTransactions;
        double totalTime;
    };
    std::vector<SimulationResult> simulation_results;

    struct ConsensusResult {
        std::string experimentName;
        int runID;
        int shardID;
        std::string role;
        int pbftRounds;
        double consensusTime;
        int messagesExchanged;
        int failedAttempts;
    };
    std::vector<ConsensusResult> consensus_results;

    struct BlockResult {
        std::string experimentName;
        int runID;
        std::string blockID;
        std::string blockType;
        int numTransactions;
        double blockCreationTime;
        std::string prevBlockHash;
    };
    std::vector<BlockResult> block_results;

    struct NodeResult {
        std::string experimentName;
        int runID;
        int finalCommitteeSize;
        int microBlocksGenerated;
        int macroBlocksGenerated;
    };
    std::vector<NodeResult> node_results;

    struct ShardResult {
        std::string experimentName;
        int runID;
        int shardID;
        int numTransactions;
    };
    std::vector<ShardResult> shard_results;

    struct ExperimentParametersResult {
        std::string experimentName;
        int runID;
        int np;
        int numShards;
        int numTransactions;
        int transactionSize;
        int seed;
    };
    std::vector<ExperimentParametersResult> experiment_parameters_results;

    bool check_if_file_empty(const std::string& filepath) {
        std::ifstream check_file(filepath);
        if (check_file.is_open()) {
            check_file.seekg(0, std::ios::end); // Position cursor at end of text with 0 offset
            return (check_file.tellg() == 0); // If cursor is still positioned at pos 0, then file is empty
        }
        return false;
    }

    void write_header(const std::string& filepath, const std::string& header) {
        std::string dir_path = filepath.substr(0, filepath.find_last_of("/"));
        mkdir(dir_path.c_str(), 0777);

        struct stat buffer;
        bool fileExists = (stat(filepath.c_str(), &buffer) == 0);

        if (!fileExists) {
            std::ofstream file(filepath, std::ios::out);
            if (file.is_open()) {
                file << header << std::endl;
            }
        } else {
            if (check_if_file_empty(filepath)) { // Only write header if it does not exist already
                std::ofstream file(filepath, std::ios::out);
                if (file.is_open()) {
                    file << header << std::endl;
                }
            }
        }

        return;
    }
}

// SimulationMetrics
void SimulationMetrics::record(const std::string& experimentName, int runID, int np, int numShards, int numTransactions, double totalTime) {
    simulation_results.push_back({experimentName, runID, np, numShards, numTransactions, totalTime});
}

void SimulationMetrics::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,NP,NumShards,NumTransactions,TotalTime,Throughput");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : simulation_results) {
        double throughput = (result.totalTime > 0) ? result.numTransactions / result.totalTime : 0;
        file << result.experimentName << ","
             << result.runID << ","
             << result.np << ","
             << result.numShards << ","
             << result.numTransactions << ","
             << result.totalTime << ","
             << throughput << std::endl;
    }
    simulation_results.clear();
}

// ConsensusMetrics
void ConsensusMetrics::record(const std::string& experimentName, int runID, int shardID, const std::string& role, int pbftRounds, double consensusTime, int messagesExchanged, int failedAttempts) {
    consensus_results.push_back({experimentName, runID, shardID, role, pbftRounds, consensusTime, messagesExchanged, failedAttempts});
}

void ConsensusMetrics::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,ShardID,Role,PBFT_Rounds,ConsensusTime,MessagesExchanged,FailedAttempts");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : consensus_results) {
        file << result.experimentName << ","
             << result.runID << ","
             << result.shardID << ","
             << result.role << ","
             << result.pbftRounds << ","
             << result.consensusTime << ","
             << result.messagesExchanged << ","
             << result.failedAttempts << std::endl;
    }
    consensus_results.clear();
}

// BlockMetrics
void BlockMetrics::record(const std::string& experimentName, int runID, const std::string& blockID, const std::string& blockType, int numTransactions, double blockCreationTime, const std::string& prevBlockHash) {
    block_results.push_back({experimentName, runID, blockID, blockType, numTransactions, blockCreationTime, prevBlockHash});
}

void BlockMetrics::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,BlockID,BlockType,NumTransactions,BlockCreationTime,PrevBlockHash");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : block_results) {
        file << result.experimentName << ","
             << result.runID << ","
             << result.blockID << ","
             << result.blockType << ","
             << result.numTransactions << ","
             << result.blockCreationTime << ","
             << result.prevBlockHash << std::endl;
    }
    block_results.clear();
}

// NodeMetrics
void NodeMetrics::record(const std::string& experimentName, int runID, int finalCommitteeSize, int microBlocksGenerated, int macroBlocksGenerated) {
    node_results.push_back({experimentName, runID, finalCommitteeSize, microBlocksGenerated, macroBlocksGenerated});
}

void NodeMetrics::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,FinalCommitteeSize,MicroBlocksGenerated,MacroBlocksGenerated");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : node_results) {
        file << result.experimentName << ","
             << result.runID << ","
             << result.finalCommitteeSize << ","
             << result.microBlocksGenerated << ","
             << result.macroBlocksGenerated << std::endl;
    }
    node_results.clear();
}

// ShardMetrics
void ShardMetrics::record(const std::string& experimentName, int runID, int shardID, int numTransactions) {
    shard_results.push_back({experimentName, runID, shardID, numTransactions});
}

void ShardMetrics::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,ShardID,NumTransactions");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : shard_results) {
        file << result.experimentName << ","
             << result.runID << ","
             << result.shardID << ","
             << result.numTransactions << std::endl;
    }
    shard_results.clear();
}

// ExperimentParameters
void ExperimentParameters::record(const std::string& experimentName, int runID, int np, int numShards, int numTransactions, int transactionSize, int seed) {
    experiment_parameters_results.push_back({experimentName, runID, np, numShards, numTransactions, transactionSize, seed});
}

void ExperimentParameters::save(const std::string& filepath) {
    write_header(filepath, "ExperimentName,RunID,NP,NumShards,NumTransactions,TransactionSize,Seed");
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
    }
    for (const auto& result : experiment_parameters_results) {
        file << result.experimentName << ","
             << result.runID << ","
             << result.np << ","
             << result.numShards << ","
             << result.numTransactions << ","
             << result.transactionSize << ","
             << result.seed << std::endl;
    }
    experiment_parameters_results.clear();
}

} // namespace util
} // namespace sbmpi
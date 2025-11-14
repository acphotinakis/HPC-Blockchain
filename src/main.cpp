#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>

#include "mpi.h"
#include "../include/sbmpi/core/node.h"
#include "../include/sbmpi/network/shard.h"
#include "../include/sbmpi/network/final_committee.h"
#include "../include/sbmpi/util/config.h"
#include "../include/sbmpi/util/logging.h"
#include "../include/sbmpi/util/timer.h"
#include "../include/sbmpi/core/transaction.h"

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    sbmpi::util::ExperimentConfig config;
    if (world_rank == 0) {
        config = sbmpi::util::loadConfig(argc, argv);
    }

    // Broadcast the configuration to all processes
    MPI_Bcast(&config, sizeof(sbmpi::util::ExperimentConfig), MPI_CHAR, 0, MPI_COMM_WORLD);

    sbmpi::util::Logger logger(world_rank, static_cast<sbmpi::util::LogLevel>(config.log_level));
    sbmpi::util::Timer timer;

    if (world_rank == 0) {
        logger.info("Simulation starting with " + std::to_string(world_size) + " total nodes.");
        logger.info("Number of shards: " + std::to_string(config.num_shards));
        logger.info("Transactions: " + std::to_string(config.total_transactions));
        timer.start();
    }

    // --- Network Partitioning ---
    int nodes_per_shard = (world_size - config.final_committee_size) / config.num_shards;
    if (nodes_per_shard < (3 * config.faulty_nodes_per_shard + 1)) {
        if (world_rank == 0) {
            logger.error("Not enough nodes per shard to tolerate " + std::to_string(config.faulty_nodes_per_shard) + " faults. Need at least " + std::to_string(3 * config.faulty_nodes_per_shard + 1));
        }
        MPI_Finalize();
        return 1;
    }

    int color = -1;
    if (world_rank < config.num_shards * nodes_per_shard) {
        color = world_rank / nodes_per_shard; // Assigns to a shard
    } else if (world_rank < config.num_shards * nodes_per_shard + config.final_committee_size) {
        color = config.num_shards; // Assigns to the final committee
    }

    MPI_Comm local_comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, world_rank, &local_comm);

    sbmpi::core::NodeRole role = sbmpi::core::NodeRole::UNASSIGNED;
    int local_rank = -1;
    if (local_comm != MPI_COMM_NULL) {
        MPI_Comm_rank(local_comm, &local_rank);
        if (color < config.num_shards) {
            role = (local_rank == 0) ? sbmpi::core::NodeRole::SHARD_LEADER : sbmpi::core::NodeRole::SHARD_MEMBER;
        } else {
            role = (local_rank == 0) ? sbmpi::core::NodeRole::FINAL_COMMITTEE_LEADER : sbmpi::core::NodeRole::FINAL_COMMITTEE_MEMBER;
        }
    }

    sbmpi::core::Node node(world_rank, local_comm, role);

    // --- Transaction Distribution ---
    if (world_rank == 0) {
        auto transactions = sbmpi::core::createMockTransactions(config.total_transactions);
        size_t tx_per_shard = transactions.size() / config.num_shards;

        for (int i = 0; i < config.num_shards; ++i) {
            int leader_global_rank = i * nodes_per_shard;
            int tx_count = (i == config.num_shards - 1) ? (transactions.size() - i * tx_per_shard) : tx_per_shard;
            
            logger.info("Sending " + std::to_string(tx_count) + " transactions to shard leader " + std::to_string(leader_global_rank));
            
            MPI_Send(&tx_count, 1, MPI_INT, leader_global_rank, 0, MPI_COMM_WORLD);
            
            std::vector<char> tx_buffer;
            for(size_t j = 0; j < tx_count; ++j) {
                auto tx_data = transactions[i * tx_per_shard + j].serialize();
                tx_buffer.insert(tx_buffer.end(), tx_data.begin(), tx_data.end());
            }
            MPI_Send(tx_buffer.data(), tx_buffer.size(), MPI_CHAR, leader_global_rank, 1, MPI_COMM_WORLD);
        }
    }

    // --- Run Simulation ---
    if (node.getRole() == sbmpi::core::NodeRole::SHARD_LEADER || node.getRole() == sbmpi::core::NodeRole::SHARD_MEMBER) {
        int final_committee_leader_rank = config.num_shards * nodes_per_shard;
        sbmpi::network::Shard shard(node, config, final_committee_leader_rank);
        shard.runMainLoop();
    } else if (node.getRole() == sbmpi::core::NodeRole::FINAL_COMMITTEE_LEADER || node.getRole() == sbmpi::core::NodeRole::FINAL_COMMITTEE_MEMBER) {
        sbmpi::network::FinalCommittee final_committee(node, config);
        final_committee.runMainLoop();
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (world_rank == 0) {
        timer.stop();
        logger.info("Simulation finished.");
        double duration = timer.getDurationSeconds();
        double tps = config.total_transactions / duration;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Total Execution Time: " << duration << " seconds" << std::endl;
        std::cout << "Throughput: " << tps << " TPS" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }

    MPI_Finalize();
    return 0;
}
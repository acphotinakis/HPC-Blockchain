/**
 * @file main.cpp
 * @brief Main entry point for the parallelized blockchain computations
 * simulation.
 *
 * This program simulates a sharded blockchain network using MPI. It initializes
 * MPI, assigns roles to nodes (Final Committee, Shard Leaders, Shard Members),
 * distributes transactions, runs a PBFT consensus protocol within each shard,
 * and aggregates microblocks into macroblocks.
 */
#include <string.h>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <vector>

#include "../include/sbmpi/consensus/pbft.h"
#include "../include/sbmpi/core/blockchain.h"
#include "../include/sbmpi/core/node.h"
#include "../include/sbmpi/core/state/transaction.h"
#include "../include/sbmpi/core/state/wallet.h"
#include "../include/sbmpi/network/committee/final_committee.h"
#include "../include/sbmpi/network/mpi_wrapper.h"
#include "../include/sbmpi/network/shard.h"
#include "../include/sbmpi/util/config.h"
#include "../include/sbmpi/util/errors.h"
#include "../include/sbmpi/util/generator.h"
#include "../include/sbmpi/util/logging.h"
#include "../include/sbmpi/util/metrics.h"
#include "../include/sbmpi/util/serialization.h"
#include "../include/sbmpi/util/timer.h"
#include "mpi.h"

// --- Constants (These align with the project structure) ---
const int FINAL_COMMITTEE_SIZE = 1;  // Fixed size for fault tolerance (3f+1)

using namespace sbmpi::util;
using namespace sbmpi::core;
using namespace sbmpi::network;
using namespace sbmpi::network::committee;

/**
 * @brief Determines the role and group assignment for a specific MPI rank.
 *
 * This function partitions the total MPI processes into a Final Committee (FC)
 * and multiple shards. The first `fc_size` ranks are reserved for the FC.
 * The remaining ranks are distributed among the shards, with the first rank
 * in each shard group designated as the Shard Leader.
 *
 * @param world_rank The global MPI rank of the current process.
 * @param world_size The total number of MPI processes.
 * @param numShards The desired number of shards.
 * @param fc_size The fixed size of the Final Committee.
 * @param shardId_out Output parameter: The assigned shard ID (or unique ID for
 * FC).
 * @param fcLeaderRank_out Output parameter: The global rank of the FC leader.
 * @return The assigned NodeRole for the current process.
 */
NodeRole determineNodeAssignment(int world_rank, int world_size, int numShards,
                                 int fc_size, int& shardId_out,
                                 int& fcLeaderRank_out)
{
  // Reserve the first FC_SIZE nodes for the Final Committee (FC).
  // The rest are for sharding. This simplifies rank calculation.
  const int FINAL_COMMITTEE_START = 0;
  const int SHARD_POOL_START      = fc_size;
  const int SHARD_POOL_SIZE       = world_size - fc_size;

  fcLeaderRank_out = FINAL_COMMITTEE_START;  // FC Leader is always Rank 0

  // 1. Assign Final Committee Nodes
  if (world_rank == FINAL_COMMITTEE_START && world_rank < fc_size) {
    shardId_out =
        numShards;  // Use 'numShards' as the unique color for the FC group
    if (world_rank == FINAL_COMMITTEE_START) {
      return NodeRole::FINAL_COMMITTEE_MEMBER;
    }
  }

  // 2. Assign Shard Pool Nodes
  if (world_rank >= SHARD_POOL_START) {
    if (SHARD_POOL_SIZE <= 0) {
      return NodeRole::UNASSIGNED;
    }

    // Calculate nodes per shard (handling remainder robustly)
    int nodesPerShardBase = SHARD_POOL_SIZE / numShards;
    int remainder         = SHARD_POOL_SIZE % numShards;
    int current_pool_rank = world_rank - SHARD_POOL_START;

    int offset = 0;
    for (int i = 0; i < numShards; ++i) {
      // Shards < remainder get one extra node
      int currentShardSize = nodesPerShardBase + (i < remainder ? 1 : 0);

      if (current_pool_rank >= offset &&
          current_pool_rank < offset + currentShardSize) {
        shardId_out   = i;  // This is the MPI_Comm_split color
        int shardRank = current_pool_rank - offset;

        if (shardRank == 0) {
          return NodeRole::SHARD_LEADER;
        } else {
          return NodeRole::SHARD_MEMBER;
        }
      }
      offset += currentShardSize;
    }
  }

  // Unassigned
  shardId_out = MPI_UNDEFINED;
  return NodeRole::UNASSIGNED;
}

/**
 * @brief Main function for the blockchain simulation.
 *
 * Orchestrates the entire simulation process, including MPI initialization,
 * node role assignment, transaction generation and distribution, parallel
 * execution of shard consensus, and final aggregation by the Final Committee.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return 0 if the simulation completes successfully, 1 otherwise.
 */
int main(int argc, char** argv)
{
  // --- Phase 1: MPI Initialization and Setup ---
  MPI_Init(&argc, &argv);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  std::unique_ptr<sbmpi::network::Shard>                     myShard = nullptr;
  std::unique_ptr<sbmpi::network::committee::FinalCommittee> finalCommittee =
      nullptr;
  std::unique_ptr<sbmpi::core::Blockchain> blockchain =
      std::make_unique<sbmpi::core::Blockchain>();

  // Logger setup
  sbmpi::util::Logger& logger = sbmpi::util::Logger::getLogger();
  logger.configure(world_rank);

  Config config;
  if (!config.parse(argc, argv)) {
    logger.fatal(ErrorCode::INVALID_ARGUMENTS,
                 "Failed to parse command line arguments.");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  const int MIN_PROCESSES = config.numShards + FINAL_COMMITTEE_SIZE;

  if (world_size < MIN_PROCESSES) {
    logger.fatal(ErrorCode::INVALID_ARGUMENTS, "Not enough nodes.");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (world_rank == 0) {
    config.print();
    logger.info("Total available nodes for sharding: " +
                std::to_string(world_size - FINAL_COMMITTEE_SIZE));
  }

  // --- Phase 2: Node Assignment and Communicator Split ---
  // Each node determines its role (Shard Leader, Shard Member, FC Member)
  // and its shard ID (or FC ID). MPI communicators are split based on these
  // IDs.
  int shard_color;
  int fc_leader_global_rank;

  NodeRole role = determineNodeAssignment(
      world_rank, world_size, config.numShards, FINAL_COMMITTEE_SIZE,
      shard_color, fc_leader_global_rank);

  MPI_Comm shard_comm = MPI_COMM_NULL;
  int      mpi_key    = (shard_color == MPI_UNDEFINED) ? 0 : world_rank;

  MPI_Comm_split(MPI_COMM_WORLD, shard_color, mpi_key, &shard_comm);

  int shard_rank = -1;
  int shard_size = 0;
  if (shard_comm != MPI_COMM_NULL) {
    MPI_Comm_rank(shard_comm, &shard_rank);
    MPI_Comm_size(shard_comm, &shard_size);
  }

  Node myNode(world_rank);
  myNode.setShardInfo(shard_color, shard_rank, role);

  // --- Phase 3: Object Instantiation ---
  // Based on the assigned role, each node instantiates either a Shard object
  // (for shard members/leaders) or a FinalCommittee object (for FC members).
  if (role == NodeRole::SHARD_LEADER || role == NodeRole::SHARD_MEMBER) {
    myShard = std::make_unique<sbmpi::network::Shard>(
        myNode.getShardId(), shard_comm, fc_leader_global_rank);

  } else if (role ==
             NodeRole::FINAL_COMMITTEE_MEMBER) {  // Note: This condition is
                                                  // redundant, but kept as is.
    finalCommittee =
        std::make_unique<sbmpi::network::committee::FinalCommittee>(shard_comm);
  }

  logger.info("Assigned Role: " + nodeRoleToString(myNode.getRole()) +
              ", Shard/FC Color: " + std::to_string(shard_color) +
              ", Local Rank: " + std::to_string(shard_rank));

  MPI_Barrier(MPI_COMM_WORLD);

  // --- Phase 4: Transaction Generation and Distribution ---
  // The root process (world_rank 0) generates mock transactions and distributes
  // them to the respective Shard Leaders.
  sbmpi::util::Timer timer;

  if (world_rank == 0) {
    logger.info("Generating wallets and transactions...");

    std::string walletJSONFilename = "wallets.json";
    std::vector<sbmpi::core::state::Wallet> allWallets;
    if (sbmpi::util::populatedFileExists(walletJSONFilename)) {
      allWallets = sbmpi::util::readWalletsJSON(walletJSONFilename);
    } else {
      allWallets = sbmpi::util::generateMockWallets(50);
      sbmpi::util::writeWalletsJSON(walletJSONFilename, allWallets);
    }
    // Create a vector containing 50 wallets, all with unique hexcodes
    // std::vector<sbmpi::core::state::Wallet> allWallets = 
    //     sbmpi::util::generateMockWallets(50); // Temp wallet count
    // sbmpi::util::writeWalletAddresses("wallets.json", allWallets);

    for (const auto& w : allWallets) {
      logger.debug("Wallet private key size: " + std::to_string(w.privateKeyRaw.size()) 
                + ", public key size: " + std::to_string(w.publicKeyRaw.size())
                + ", address: " + w.address);
    }

    std::string transactionJSONFilename = 
      "transactions_" + std::to_string(config.numTransactions) + ".json";
    logger.info(transactionJSONFilename);
    std::vector<sbmpi::core::state::Transaction> allTransactions;
    if (sbmpi::util::populatedFileExists(transactionJSONFilename)) {
      logger.info("Transactions file exists, reading transactions.");
      allTransactions = sbmpi::util::readTransactionsJSON(transactionJSONFilename);
      logger.info(std::to_string(allTransactions.size()));
    } else {
      logger.info("Transactions file does not exist, creating transactions.");
      allTransactions = 
        sbmpi::util::generateMockTransactions(config.numTransactions, allWallets);
      sbmpi::util::writeTransactionsJSON(transactionJSONFilename, allTransactions);
    }

    sbmpi::util::ExperimentParameters::record(
        "total_simulation", config.runID, world_size, config.numShards,
        config.numTransactions, config.transactionSize, config.seed);

    // Start timer after mock transactions have been generated
    timer.start();

    logger.info("Partitioning and distributing transactions...");
    std::vector<std::vector<sbmpi::core::state::Transaction>> partitioned_txs(
        config.numShards);
    for (size_t i = 0; i < allTransactions.size(); i++) {
      // Evenly distribute transactions amongst shards
      int shardId = i % config.numShards;
      partitioned_txs[shardId].push_back(allTransactions[i]);
    }

    for (int shardId = 0; shardId < config.numShards; ++shardId) {
      sbmpi::util::ShardMetrics::record("total_simulation", config.runID,
                                        shardId,
                                        partitioned_txs[shardId].size());
      // Calculate global rank of Shard Leader
      // Logic: FC takes first N slots. Remaining slots are shards.
      // Shard Pool Size = World - FC.
      // We must replicate logic from determineNodeAssignment to identify leader
      // rank. Simplified calculation:
      int shardPoolSize     = world_size - FINAL_COMMITTEE_SIZE;
      int nodesPerShardBase = shardPoolSize / config.numShards;
      int remainder         = shardPoolSize % config.numShards;

      int offset = FINAL_COMMITTEE_SIZE;  // Start after FC
      for (int k = 0; k < shardId; ++k) {
        offset += nodesPerShardBase + (k < remainder ? 1 : 0);
      }
      int shardLeaderGlobalRank = offset;  // The first node in the shard block

      if (partitioned_txs[shardId].empty()) continue;

      std::vector<char> buffer;
      auto&             tx_list = partitioned_txs[shardId];
      pack(static_cast<int>(tx_list.size()), buffer);
      for (const auto& tx : tx_list) {
        std::vector<char> tx_data = tx.serialize();
        pack(static_cast<int>(tx_data.size()), buffer);
        buffer.insert(buffer.end(), tx_data.begin(), tx_data.end());
      }

      sbmpi::network::send(buffer, shardLeaderGlobalRank, 0, MPI_COMM_WORLD);
      logger.debug("Sent " + std::to_string(partitioned_txs[shardId].size()) +
                   " txs to Leader " + std::to_string(shardLeaderGlobalRank));
    }
  }

  // --- Phase 5: Parallel Execution ---
  // Shard nodes run their local PBFT consensus to produce microblocks.
  // Final Committee members collect these microblocks and assemble a
  // macroblock.
  if (myShard) {
    // This will now internally recv transactions (if leader), run PBFT, and
    // send result
    myShard->runConsensus(config.runID);
  }

  if (finalCommittee) {
    // Calculate the global ranks of all shard leaders so FC knows who to listen
    // to
    std::vector<int> shardLeaderRanks;
    int              shardPoolSize     = world_size - FINAL_COMMITTEE_SIZE;
    int              nodesPerShardBase = shardPoolSize / config.numShards;
    int              remainder         = shardPoolSize % config.numShards;
    int              offset            = FINAL_COMMITTEE_SIZE;

    for (int i = 0; i < config.numShards; ++i) {
      shardLeaderRanks.push_back(offset);
      logger.info("Shard leader added: " + std::to_string(offset));
      offset += nodesPerShardBase + (i < remainder ? 1 : 0);
    }

    // Pass the calculated ranks to collectMicroBlocks
    std::vector<sbmpi::core::blocks::MicroBlock> collectedMicroBlocks;
    if (world_rank ==
        0) {  // Only the leader needs to receive blocks from shard leaders
      collectedMicroBlocks =
          finalCommittee->collectMicroBlocks(shardLeaderRanks);
      logger.info("Collected all microblocks! Size: " +
                  std::to_string(collectedMicroBlocks.size()));

      // Pass in previous block info in order to construct prev block
      // information for new block
      sbmpi::core::blocks::MacroBlock macroBlock =
          finalCommittee->assembleMacroBlock(
              collectedMicroBlocks, blockchain->getLatestBlock(), config.runID);

      blockchain->addBlock(
          std::make_unique<sbmpi::core::blocks::MacroBlock>(macroBlock));
      logger.info("MacroBlock assembled and added to blockchain.");

      sbmpi::util::NodeMetrics::record("total_simulation", config.runID,
                                       FINAL_COMMITTEE_SIZE,
                                       collectedMicroBlocks.size(), 1);
    }
  }

  // Synchronize processes before continuing to final step
  MPI_Barrier(MPI_COMM_WORLD);

  // --- Phase 6: Finalization ---
  // The root process records simulation metrics and cleans up MPI resources.
  if (world_rank == 0) {
    timer.stop();
    double elapsed_time = timer.elapsedSeconds();
    sbmpi::util::SimulationMetrics::record(
        "total_simulation", config.runID, world_size, config.numShards,
        config.numTransactions, elapsed_time);
    sbmpi::util::SimulationMetrics::save("metrics/simulation_metrics.csv");
    sbmpi::util::ConsensusMetrics::save("metrics/consensus_metrics.csv");
    sbmpi::util::BlockMetrics::save("metrics/block_metrics.csv");
    sbmpi::util::NodeMetrics::save("metrics/node_metrics.csv");
    sbmpi::util::ShardMetrics::save("metrics/shard_metrics.csv");
    sbmpi::util::ExperimentParameters::save(
        "metrics/experiment_parameters.csv");

    // Print out the current blockchain for a given run
    const std::vector<std::unique_ptr<sbmpi::core::blocks::Block>>& chain =
        blockchain->getBlockchain();
    for (size_t bindex = 0; bindex < chain.size(); bindex++) {
      if (chain[bindex]) {
        std::stringstream ss;
        ss << "[Block " << std::to_string(bindex)
           << "] | Hash: " << chain[bindex]->getHash()
           << " | Number of Transactions: "
           << std::to_string(chain[bindex]->transactions.size());
        logger.info(ss.str());
      }
    }

    logger.info("Simulation finished in " + std::to_string(elapsed_time) +
                " seconds.");
  }

  if (shard_comm != MPI_COMM_NULL && shard_comm != MPI_COMM_WORLD) {
    MPI_Comm_free(&shard_comm);
  }

  MPI_Finalize();
  return 0;
}
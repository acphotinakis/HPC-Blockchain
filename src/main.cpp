#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include "../include/sbmpi/consensus/pbft.h"
#include "../include/sbmpi/core/blockchain.h"
#include "../include/sbmpi/core/node.h"
#include "../include/sbmpi/core/state/transaction.h"
#include "../include/sbmpi/network/committee/final_committee.h"
#include "../include/sbmpi/network/mpi_wrapper.h"
#include "../include/sbmpi/network/shard.h"
#include "../include/sbmpi/util/config.h"
#include "../include/sbmpi/util/errors.h"
#include "../include/sbmpi/util/generator.h"  // For mock data generation
#include "../include/sbmpi/util/logging.h"
#include "../include/sbmpi/util/metrics.h"
#include "../include/sbmpi/util/timer.h"
#include "mpi.h"

// --- Constants (These align with the project structure) ---
const int FINAL_COMMITTEE_SIZE = 4;  // Fixed size for fault tolerance (3f+1)

using namespace sbmpi::util;
using namespace sbmpi::core;
using namespace sbmpi::network;
using namespace sbmpi::network::committee;

/**
 * @brief Determines the role and group assignment for a specific MPI rank.
 *
 * This function implements the robust node allocation strategy to handle
 * remainders and ensure the Final Committee is correctly assigned.
 *
 * @param world_rank Current process's rank.
 * @param world_size Total number of processes.
 * @param numShards Number of parallel shards requested.
 * @param fc_size The size reserved for the Final Committee.
 * @param shardId_out Output: The color (shard ID) for MPI_Comm_split.
 * @param fcLeaderRank_out Output: The global rank of the Final Committee
 * Leader.
 * @return The determined NodeRole for the current rank.
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
  if (world_rank >= FINAL_COMMITTEE_START && world_rank < fc_size) {
    shardId_out =
        numShards;  // Use 'numShards' as the unique color for the FC group
    if (world_rank == FINAL_COMMITTEE_START) {
      return NodeRole::FINAL_COMMITTEE_MEMBER;
    } else {
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

  // Unassigned (if allocation logic fails or rank is out of bounds)
  shardId_out = MPI_UNDEFINED;
  return NodeRole::UNASSIGNED;
}

int main(int argc, char** argv)
{
  // --- Phase 1: MPI Initialization and Setup ---
  MPI_Init(&argc, &argv);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Use unique_ptr for automatic memory management (Fix 4)
  std::unique_ptr<sbmpi::network::Shard>                     myShard = nullptr;
  std::unique_ptr<sbmpi::network::committee::FinalCommittee> finalCommittee =
      nullptr;

  // Logger setup
  sbmpi::util::Logger& logger = sbmpi::util::Logger::getLogger();
  // Set the rank immediately
  logger.configure(world_rank);

  Config config;
  if (!config.parse(argc, argv)) {
    logger.fatal(ErrorCode::INVALID_ARGUMENTS,
                 "Failed to parse command line arguments.");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  // Validate Allocation Parameters
  const int MIN_PROCESSES = config.numShards + FINAL_COMMITTEE_SIZE;

  if (world_size < MIN_PROCESSES) {
    logger.fatal(ErrorCode::INVALID_ARGUMENTS,
                 "Total nodes (%d) must be at least %d (Shards + FC Size).",
                 world_size, MIN_PROCESSES);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (world_rank == 0) {
    config.print();
    logger.info("Total available nodes for sharding: " +
                std::to_string(world_size - FINAL_COMMITTEE_SIZE));
  }

  // --- Phase 2: Node Assignment and Communicator Split ---
  int shard_color;
  int fc_leader_global_rank;

  // Determine identity based on rank
  NodeRole role = determineNodeAssignment(
      world_rank, world_size, config.numShards, FINAL_COMMITTEE_SIZE,
      shard_color, fc_leader_global_rank);

  // CRITICAL FIX: All processes must call MPI_Comm_split unconditionally
  // (Fix 1.1)
  MPI_Comm shard_comm = MPI_COMM_NULL;
  int      mpi_key    = (shard_color == MPI_UNDEFINED) ? 0 : world_rank;

  // Split the world into private rooms based on 'shard_color'
  MPI_Comm_split(MPI_COMM_WORLD, shard_color, mpi_key, &shard_comm);

  // Get new local rank within the private room
  int shard_rank = -1;
  int shard_size = 0;
  if (shard_comm != MPI_COMM_NULL) {
    MPI_Comm_rank(shard_comm, &shard_rank);
    MPI_Comm_size(shard_comm, &shard_size);
  }

  // Persist identity in Node object
  Node myNode(world_rank, shard_rank, role, shard_comm);

  // --- Phase 3: Object Instantiation ---
  if (role == NodeRole::SHARD_LEADER || role == NodeRole::SHARD_MEMBER) {
    // Shard members get a Shard object
    myShard = std::make_unique<sbmpi::network::Shard>(
        myNode.getShardId(), shard_comm, fc_leader_global_rank);

  } else if (role == NodeRole::FINAL_COMMITTEE_MEMBER ||
             role == NodeRole::FINAL_COMMITTEE_MEMBER) {
    // Final committee nodes get a FinalCommittee object
    finalCommittee =
        std::make_unique<sbmpi::network::committee::FinalCommittee>(
            shard_comm, config.numShards);
  }

  logger.info("Assigned Role: " + std::to_string(static_cast<int>(role)) +
              ", Shard/FC Color: " + std::to_string(shard_color) +
              ", Local Rank: " + std::to_string(shard_rank));

  // --- Phase 4: Transaction Generation and Distribution (Fix 3) ---
  sbmpi::util::Timer timer;

  if (world_rank == 0) {
    // Only Rank 0 (Root/Client) generates and distributes transactions
    logger.info("Generating and distributing transactions...");
    timer.start();

    // 1. Generate all transactions (Mock Data)
    std::vector<sbmpi::core::state::Transaction> all_transactions =
        sbmpi::util::generateMockTransactions(config.numTransactions);

    // 2. Partition the transactions based on the number of shards
    std::vector<std::vector<sbmpi::core::state::Transaction>> partitioned_txs(
        config.numShards);
    for (const auto& tx : all_transactions) {
      // Deterministic partitioning: ID % Shards
      int shardId = tx.id % config.numShards;
      partitioned_txs[shardId].push_back(tx);
    }

    // 3. Send transaction sets to the respective Shard Leaders
    for (int shardId = 0; shardId < config.numShards; ++shardId) {
      // Calculate the global rank of the Shard Leader
      // Note: This relies on the 'determineNodeAssignment' logic
      int shardLeaderGlobalRank =
          FINAL_COMMITTEE_SIZE +
          (shardId * (world_size - FINAL_COMMITTEE_SIZE) / config.numShards);

      if (partitioned_txs[shardId].empty()) continue;

      // Send using MPIWrapper
      sbmpi::network::send(partitioned_txs[shardId], shardLeaderGlobalRank, 0,
                           MPI_COMM_WORLD);
      logger.debug("Sent " + std::to_string(partitioned_txs[shardId].size()) +
                   " transactions to Shard Leader at rank " +
                   std::to_string(shardLeaderGlobalRank));
    }
  }

  // --- Phase 5: Parallel Execution ---

  if (myShard) {
    // Shards run PBFT consensus and send MicroBlock to FC Leader (Rank 0)
    // Shard::runConsensus() must implement the logic to receive its
    // transactions first.
    myShard->runConsensus();
  }

  if (finalCommittee) {
    // Final committee collects, assembles, and commits
    std::vector<sbmpi::core::blocks::MicroBlock> collectedMicroBlocks =
        finalCommittee->collectMicroBlocks(
            config.numShards);  // Collects from N shards

    if (myNode.getRole() == NodeRole::FINAL_COMMITTEE_MEMBER) {
      sbmpi::core::blocks::MacroBlock macroBlock =
          finalCommittee->assembleMacroBlock(collectedMicroBlocks);

      // Add block to the chain
      sbmpi::core::Blockchain blockchain;
      blockchain.addBlock(
          std::make_unique<sbmpi::core::blocks::MacroBlock>(macroBlock));
      logger.info("MacroBlock assembled and added to blockchain.");
    }
  }

  // --- Phase 6: Finalization ---
  // Only Rank 0 handles the final timing and metrics
  if (world_rank == 0) {
    timer.stop();
    double elapsed_time = timer.elapsedSeconds();
    sbmpi::util::Metrics::recordTime("total_simulation", elapsed_time,
                                     config.numTransactions);
    sbmpi::util::Metrics::save("metrics.csv");
    logger.info("Simulation finished in " + std::to_string(elapsed_time) +
                " seconds.");
  }

  // Clean up the shard communicator
  if (shard_comm != MPI_COMM_NULL && shard_comm != MPI_COMM_WORLD) {
    MPI_Comm_free(&shard_comm);
  }

  MPI_Finalize();
  return 0;
}

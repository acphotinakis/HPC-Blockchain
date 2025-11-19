#include <string.h>
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
#include "../include/sbmpi/util/generator.h"
#include "../include/sbmpi/util/logging.h"
#include "../include/sbmpi/util/metrics.h"
#include "../include/sbmpi/util/serialization.h"
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

  // Unassigned
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

  std::unique_ptr<sbmpi::network::Shard>                     myShard = nullptr;
  std::unique_ptr<sbmpi::network::committee::FinalCommittee> finalCommittee =
      nullptr;

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
  if (role == NodeRole::SHARD_LEADER || role == NodeRole::SHARD_MEMBER) {
    myShard = std::make_unique<sbmpi::network::Shard>(
        myNode.getShardId(), shard_comm, fc_leader_global_rank);

  } else if (role == NodeRole::FINAL_COMMITTEE_MEMBER ||
             role == NodeRole::FINAL_COMMITTEE_MEMBER) {
    finalCommittee =
        std::make_unique<sbmpi::network::committee::FinalCommittee>(shard_comm);
  }

  logger.info("Assigned Role: " + std::to_string(static_cast<int>(role)) +
              ", Shard/FC Color: " + std::to_string(shard_color) +
              ", Local Rank: " + std::to_string(shard_rank));

  // --- Phase 4: Transaction Generation and Distribution ---
  sbmpi::util::Timer timer;

  if (world_rank == 0) {
    logger.info("Generating and distributing transactions...");
    timer.start();

    std::vector<sbmpi::core::state::Transaction> all_transactions =
        sbmpi::util::generateMockTransactions(config.numTransactions);

    std::vector<std::vector<sbmpi::core::state::Transaction>> partitioned_txs(
        config.numShards);
    for (const auto& tx : all_transactions) {
      // Use std::stoull to parse ID (safer than stoi if IDs get large)
      // Fallback to 0 if ID parsing fails (though generator makes safe IDs)
      uint64_t txIdVal = 0;
      try {
        txIdVal = std::stoull(tx.id);
      } catch (...) {
      }

      int shardId = txIdVal % config.numShards;
      partitioned_txs[shardId].push_back(tx);
    }

    for (int shardId = 0; shardId < config.numShards; ++shardId) {
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

  if (myShard) {
    // This will now internally recv transactions (if leader), run PBFT, and
    // send result
    myShard->runConsensus();
  }

  if (finalCommittee) {
    // FIX: Calculate the global ranks of all shard leaders so FC knows who to
    // listen to
    std::vector<int> shardLeaderRanks;
    int              shardPoolSize     = world_size - FINAL_COMMITTEE_SIZE;
    int              nodesPerShardBase = shardPoolSize / config.numShards;
    int              remainder         = shardPoolSize % config.numShards;
    int              offset            = FINAL_COMMITTEE_SIZE;

    for (int i = 0; i < config.numShards; ++i) {
      shardLeaderRanks.push_back(offset);
      offset += nodesPerShardBase + (i < remainder ? 1 : 0);
    }

    // Pass the calculated ranks to collectMicroBlocks
    std::vector<sbmpi::core::blocks::MicroBlock> collectedMicroBlocks =
        finalCommittee->collectMicroBlocks(shardLeaderRanks);

    if (myNode.getRole() == NodeRole::FINAL_COMMITTEE_MEMBER) {
      sbmpi::core::blocks::MacroBlock macroBlock =
          finalCommittee->assembleMacroBlock(collectedMicroBlocks);

      sbmpi::core::Blockchain blockchain;
      blockchain.addBlock(
          std::make_unique<sbmpi::core::blocks::MacroBlock>(macroBlock));
      logger.info("MacroBlock assembled and added to blockchain.");
    }
  }

  // --- Phase 6: Finalization ---
  if (world_rank == 0) {
    timer.stop();
    double elapsed_time = timer.elapsedSeconds();
    sbmpi::util::Metrics::recordTime("total_simulation", elapsed_time,
                                     config.numTransactions);
    sbmpi::util::Metrics::save("metrics.csv");
    logger.info("Simulation finished in " + std::to_string(elapsed_time) +
                " seconds.");
  }

  if (shard_comm != MPI_COMM_NULL && shard_comm != MPI_COMM_WORLD) {
    MPI_Comm_free(&shard_comm);
  }

  MPI_Finalize();
  return 0;
}
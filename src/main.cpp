#include <iostream>
#include <memory>
#include <vector>

#include "../include/sbmpi/consensus/pbft.h"
#include "../include/sbmpi/core/blockchain.h"
#include "../include/sbmpi/core/node.h"
#include "../include/sbmpi/core/state/transaction.h"
#include "../include/sbmpi/network/committee/final_committee.h"
#include "../include/sbmpi/network/shard.h"
#include "../include/sbmpi/util/config.h"
#include "../include/sbmpi/util/errors.h"
#include "../include/sbmpi/util/logging.h"
#include "../include/sbmpi/util/metrics.h"
#include "../include/sbmpi/util/timer.h"
#include "mpi.h"

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  sbmpi::util::Config config;
  if (!config.parse(argc, argv)) {
    sbmpi::util::fatal(sbmpi::util::ErrorCode::INVALID_ARGUMENTS,
                       "Failed to parse command line arguments.");
  }

  if (world_rank == 0) {
    config.print();
  }

  // Basic setup for nodes and shards
  int numShards = config.numShards;
  if (world_size < numShards) {
    sbmpi::util::fatal(
        sbmpi::util::ErrorCode::INVALID_ARGUMENTS,
        "Number of nodes must be greater than or equal to number "
        "of shards.");
  }

  // Divide nodes into shards and a final committee
  // For simplicity, let's assume one leader per shard and remaining nodes as
  // members. The last few nodes can form the final committee.
  int nodesPerShard =
      (world_size - 1) / numShards;  // -1 for final committee leader
  int finalCommitteeSize = 1;        // For now, a single final committee leader

  sbmpi::core::Node                          myNode(world_rank);
  sbmpi::network::Shard*                     myShard        = nullptr;
  sbmpi::network::committee::FinalCommittee* finalCommittee = nullptr;

  if (world_rank < numShards * nodesPerShard) {
    // Shard member
    int shardId    = world_rank / nodesPerShard;
    int shardRank  = world_rank % nodesPerShard;
    int leaderRank = shardId * nodesPerShard;  // First node in shard is leader

    sbmpi::core::NodeRole role = sbmpi::core::NodeRole::SHARD_MEMBER;
    if (shardRank == 0) {
      role = sbmpi::core::NodeRole::SHARD_LEADER;
    }
    myNode.setShardInfo(shardId, shardRank, role);

    // Create a communicator for the shard
    MPI_Comm shard_comm;
    MPI_Comm_split(MPI_COMM_WORLD, shardId, world_rank, &shard_comm);
    myShard = new sbmpi::network::Shard(shardId, shard_comm, leaderRank);

    sbmpi::util::Logger::log(sbmpi::util::LogLevel::INFO,
                             "Node " + std::to_string(world_rank) +
                                 " is shard member of shard " +
                                 std::to_string(shardId) + " with rank " +
                                 std::to_string(shardRank));

  } else if (world_rank ==
             world_size - 1) {  // Last node is final committee leader
    myNode.setShardInfo(-1, -1, sbmpi::core::NodeRole::FINAL_COMMITTEE_MEMBER);
    finalCommittee =
        new sbmpi::network::committee::FinalCommittee(MPI_COMM_WORLD);
    sbmpi::util::Logger::log(
        sbmpi::util::LogLevel::INFO,
        "Node " + std::to_string(world_rank) + " is final committee leader.");
  }

  sbmpi::util::Timer timer;
  timer.start();

  // Simulation loop
  sbmpi::core::Blockchain blockchain;

  if (myShard) {
    // Shard members generate and process transactions
    std::vector<sbmpi::core::state::Transaction> transactions;
    for (int i = 0; i < config.numTransactions / world_size; ++i) {
      sbmpi::core::state::Transaction tx(
          "sender_" + std::to_string(world_rank),
          "receiver_" + std::to_string(world_rank), 1.0);
      tx.sign("private_key_" + std::to_string(world_rank));
      transactions.push_back(tx);
      myShard->addTransaction(tx);
    }

    // Run PBFT consensus for microblocks
    sbmpi::consensus::PBFT          pbft(myShard->getCommunicator(),
                                         myNode.getShardRank(), 0, nodesPerShard);
    sbmpi::core::blocks::MicroBlock microBlock = pbft.run(transactions);

    // Send microblock to final committee
    if (myNode.getRole() == sbmpi::core::NodeRole::SHARD_LEADER) {
      std::vector<char> microBlockData = microBlock.serialize();
      sbmpi::network::send(microBlockData, world_size - 1, 0, MPI_COMM_WORLD);
    }
  }

  if (finalCommittee) {
    // Final committee collects microblocks and assembles macroblock
    std::vector<sbmpi::core::blocks::MicroBlock> collectedMicroBlocks =
        finalCommittee->collectMicroBlocks(numShards);
    sbmpi::core::blocks::MacroBlock macroBlock =
        finalCommittee->assembleMacroBlock(collectedMicroBlocks);
    blockchain.addBlock(
        std::make_unique<sbmpi::core::blocks::MacroBlock>(macroBlock));
    sbmpi::util::Logger::log(sbmpi::util::LogLevel::INFO,
                             "MacroBlock assembled and added to blockchain.");
  }

  timer.stop();

  double elapsed_time = timer.elapsedSeconds();
  if (world_rank == 0) {
    sbmpi::util::Metrics::recordTime("total_simulation", elapsed_time,
                                     config.numTransactions);
    sbmpi::util::Metrics::save("metrics.csv");
    sbmpi::util::Logger::log(
        sbmpi::util::LogLevel::INFO,
        "Simulation finished in " + std::to_string(elapsed_time) + " seconds.");
  }

  delete myShard;
  delete finalCommittee;

  MPI_Finalize();
  return 0;
}

# src/main.cpp

```cpp
/**
 * @file main.cpp
 * @brief Main entry point for the parallelized blockchain computations simulation.
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
 * @param shardId_out Output parameter: The assigned shard ID (or unique ID for FC).
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
  // and its shard ID (or FC ID). MPI communicators are split based on these IDs.
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

  } else if (role == NodeRole::FINAL_COMMITTEE_MEMBER ||
             role == NodeRole::FINAL_COMMITTEE_MEMBER) { // Note: This condition is redundant, but kept as is.
    finalCommittee =
        std::make_unique<sbmpi::network::committee::FinalCommittee>(shard_comm);
  }

  logger.info("Assigned Role: " + std::to_string(static_cast<int>(role)) +
              ", Shard/FC Color: " + std::to_string(shard_color) +
              ", Local Rank: " + std::to_string(shard_rank));

  // --- Phase 4: Transaction Generation and Distribution ---
  // The root process (world_rank 0) generates mock transactions and distributes
  // them to the respective Shard Leaders.
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
  // Shard nodes run their local PBFT consensus to produce microblocks.
  // Final Committee members collect these microblocks and assemble a macroblock.
  if (myShard) {
    // This will now internally recv transactions (if leader), run PBFT, and
    // send result
    myShard->runConsensus();
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
  // The root process records simulation metrics and cleans up MPI resources.
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
```

# src/consensus/pbft_messages.cpp

```cpp
/**
 * @file pbft_messages.cpp
 * @brief Implements serialization and deserialization for PBFT messages.
 */
#include "../../include/sbmpi/consensus/pbft_messages.h"
#include <vector>
#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace consensus
  {

    /**
     * @brief Serializes a PBFTMessage into a vector of characters for network transmission.
     * @param msg The PBFTMessage to serialize.
     * @return A std::vector<char> containing the serialized message data.
     */
    std::vector<char> serializeMessage(const PBFTMessage& msg)
    {
      std::vector<char> buffer;
      util::pack(static_cast<int>(msg.type), buffer);
      util::pack(msg.senderId, buffer);
      util::pack(msg.blockHash, buffer);
      return buffer;
    }

    /**
     * @brief Deserializes a vector of characters back into a PBFTMessage object.
     * @param data The std::vector<char> containing the serialized message data.
     * @return A PBFTMessage object reconstructed from the provided data.
     */
    PBFTMessage deserializeMessage(const std::vector<char>& data)
    {
      PBFTMessage msg;
      int offset = 0;
      msg.type = static_cast<PBFTMessageType>(util::unpack_int(data, offset));
      msg.senderId = util::unpack_int(data, offset);
      msg.blockHash = util::unpack_string(data, offset);
      return msg;
    }

  } // namespace consensus
} // namespace sbmpi

```

# src/consensus/pbft.cpp

```cpp
/**
 * @file pbft.cpp
 * @brief Implements the Practical Byzantine Fault Tolerance (PBFT) consensus protocol.
 */
#include "../../include/sbmpi/consensus/pbft.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../include/sbmpi/consensus/pbft_messages.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/util/logging.h" // Added logging

namespace sbmpi
{
  namespace consensus
  {

    /**
     * @brief Implements the Practical Byzantine Fault Tolerance (PBFT) consensus protocol.
     *
     * This class manages the PBFT phases (Pre-Prepare, Prepare, Commit) to reach
     * consensus on a microblock within a shard. It handles message broadcasting,
     * quorum checking, and fault tolerance calculations.
     */
    PBFT::PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes)
        : communicator(comm),
          myRank(rank),
          leaderRank(leaderRank),
          numNodes(numNodes)
    {
      // PBFT tolerance: f = (n-1)/3. Quorum = 2f + 1.
      maxFaultyNodes = (numNodes - 1) / 3;
    }

    /**
     * @brief Broadcasts a PBFT message to all other nodes in the communicator.
     * @param msg The PBFTMessage to be broadcast.
     */
    void PBFT::broadcastMessage(const PBFTMessage& msg)
    {
      std::vector<char> data = serializeMessage(msg);
      for (int i = 0; i < numNodes; ++i) {
        if (i != myRank) {
          network::send(data, i, 0, communicator);
        }
      }
    }

    /**
     * @brief Initiates the Pre-Prepare phase of PBFT.
     *
     * The leader node broadcasts the proposed block and a PRE-PREPARE message
     * to all replicas.
     * @param block The MicroBlock proposed by the leader.
     */
    void PBFT::prePrepare(const core::blocks::MicroBlock& block)
    {
      PBFTMessage msg;
      msg.type = PBFTMessageType::PRE_PREPARE;
      msg.senderId = myRank;
      msg.blockHash = block.getHash();

      // 1. Broadcast the full block first (simplification for simulation)
      std::vector<char> blockData = block.serialize();
      network::bcast(blockData, leaderRank, communicator);

      // 2. Broadcast the Pre-Prepare consensus message
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: Broadcasting PRE-PREPARE for block " +
                                      msg.blockHash);
      broadcastMessage(msg);
    }

    /**
     * @brief Sends a PREPARE message for a given block hash.
     *
     * Replicas send this message after receiving a valid PRE-PREPARE message
     * and verifying the proposed block.
     * @param blockHash The hash of the block being prepared.
     */
    void PBFT::prepare(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type = PBFTMessageType::PREPARE;
      msg.senderId = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    /**
     * @brief Sends a COMMIT message for a given block hash.
     *
     * Nodes send this message after collecting a quorum of PREPARE messages.
     * @param blockHash The hash of the block being committed.
     */
    void PBFT::commit(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type = PBFTMessageType::COMMIT;
      msg.senderId = myRank;
      msg.blockHash = blockHash;
      broadcastMessage(msg);
    }

    /**
     * @brief Executes the PBFT consensus protocol to agree on a new microblock.
     *
     * This method orchestrates the entire PBFT process, including Pre-Prepare,
     * Prepare, and Commit phases. The leader proposes a block, and replicas
     * validate and agree upon it.
     *
     * @param transactions A vector of transactions to be included in the proposed block.
     * @param previousHash The hash of the previous block in the blockchain.
     * @return The MicroBlock that has reached consensus.
     */
    core::blocks::MicroBlock PBFT::run(
        const std::vector<core::state::Transaction>& transactions,
        const std::string& previousHash)
    {
      core::blocks::MicroBlock block;

      // --- PHASE 0: PRE-PREPARE ---
      if (myRank == leaderRank) {
        std::string merkleRoot = util::merkle(transactions);
        // Use the passed previousHash instead of the placeholder
        block.header = core::blocks::BlockHeader(1, previousHash, merkleRoot);
        block.transactions = transactions;

        util::Logger::getLogger().info("PBFT Leader: Proposing block with " +
                                       std::to_string(transactions.size()) +
                                       " transactions.");
        prePrepare(block);
      } else {
        // Replicas receive the block content
        std::vector<char> blockData;
        network::bcast(blockData, leaderRank, communicator);
        block.deserialize(blockData);
        util::Logger::getLogger().debug(
            "PBFT Replica: Received block proposal.");
      }

      std::string proposedBlockHash = block.getHash();
      int quorum = 2 * maxFaultyNodes + 1;

      // --- PHASE 1: PREPARE ---
      if (myRank != leaderRank) {
        prepare(proposedBlockHash);
      }

      int prepareCount = 0;
      if (myRank == leaderRank)
        prepareCount++;
      else
        prepareCount++;

      std::set<int> prepareVoters;
      prepareVoters.insert(myRank);

      while (prepareCount < quorum) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int source = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage msg = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::PREPARE &&
            msg.blockHash == proposedBlockHash) {
          if (prepareVoters.find(msg.senderId) == prepareVoters.end()) {
            prepareVoters.insert(msg.senderId);
            prepareCount++;
          }
        }
      }
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: PREPARED (Quorum " +
                                      std::to_string(prepareCount) + ")");

      // --- PHASE 2: COMMIT ---
      commit(proposedBlockHash);

      int commitCount = 1;
      std::set<int> commitVoters;
      commitVoters.insert(myRank);

      while (commitCount < quorum) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int source = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage msg = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::COMMIT &&
            msg.blockHash == proposedBlockHash) {
          if (commitVoters.find(msg.senderId) == commitVoters.end()) {
            commitVoters.insert(msg.senderId);
            commitCount++;
          }
        }
      }
      util::Logger::getLogger().debug("PBFT [Rank " + std::to_string(myRank) +
                                      "]: COMMITTED (Quorum " +
                                      std::to_string(commitCount) + ")");

      // --- CONSENSUS REACHED ---
      return block;
    }

  } // namespace consensus
} // namespace sbmpi

```

# src/core/blockchain.cpp

```cpp
/**
 * @file blockchain.cpp
 * @brief Implements the Blockchain class for managing a chain of blocks.
 */
#include "../../include/sbmpi/core/blockchain.h"

#include <memory>
#include <vector>

#include "../../include/sbmpi/core/state/genesis.h"

namespace sbmpi
{
  namespace core
  {

    /**
     * @brief Represents a blockchain, managing a sequence of blocks.
     *
     * This class provides functionality to add blocks, retrieve blocks by height,
     * get the latest block, and validate the integrity of the chain.
     */
    Blockchain::Blockchain()
    {
      createGenesisBlock();
    }

    /**
     * @brief Creates and adds the genesis block to the blockchain.
     *
     * This is typically the first block in the chain, with predefined properties.
     */
    void Blockchain::createGenesisBlock()
    {
      chain.push_back(state::createGenesisBlock());
    }

    /**
     * @brief Adds a new block to the blockchain after basic validation.
     *
     * The block is added only if its previous hash matches the latest block's hash
     * and its height is one greater than the latest block's height.
     * @param block A unique pointer to the block to be added.
     */
    void Blockchain::addBlock(std::unique_ptr<blocks::Block> block)
    {
      if (block) {
        // Basic validation
        const blocks::Block* latest = getLatestBlock();
        if (latest && latest->getHash() == block->header.previousHash &&
            latest->header.height + 1 == block->header.height) {
          chain.push_back(std::move(block));
        }
      }
    }

    /**
     * @brief Retrieves a block from the blockchain by its height.
     * @param height The height (index) of the block to retrieve.
     * @return A pointer to the Block if found, nullptr otherwise.
     */
    const blocks::Block* Blockchain::getBlock(int height) const
    {
      if (height >= 0 && height < chain.size()) {
        return chain[height].get();
      }
      return nullptr;
    }

    /**
     * @brief Retrieves the latest block in the blockchain.
     * @return A pointer to the latest Block if the chain is not empty, nullptr otherwise.
     */
    const blocks::Block* Blockchain::getLatestBlock() const
    {
      if (chain.empty()) {
        return nullptr;
      }
      return chain.back().get();
    }

    /**
     * @brief Validates the integrity of the blockchain.
     *
     * Checks if each block's previous hash matches the hash of the preceding block
     * and if block heights are sequential.
     * @return True if the blockchain is valid, false otherwise.
     */
    bool Blockchain::validate() const
    {
      if (chain.size() <= 1) {
        return true;
      }
      for (size_t i = 1; i < chain.size(); ++i) {
        const auto& current = chain[i];
        const auto& previous = chain[i - 1];
        if (current->header.previousHash != previous->getHash()) {
          return false;
        }
        if (current->header.height != previous->header.height + 1) {
          return false;
        }
      }
      return true;
    }

    /**
     * @brief Returns the current height of the blockchain.
     * @return The height of the latest block (0-indexed), or -1 if the chain is empty.
     */
    int Blockchain::getHeight() const
    {
      return chain.empty() ? -1 : static_cast<int>(chain.size()) - 1;
    }

  } // namespace core
} // namespace sbmpi
```

# src/core/node.cpp

```cpp
/**
 * @file node.cpp
 * @brief Implements the Node class, representing a participant in the blockchain network.
 */
#include "../../include/sbmpi/core/node.h"
#include <string>

namespace sbmpi
{
  namespace core
  {

    /**
     * @brief Represents a node in the blockchain network.
     *
     * Each node has a global rank, and potentially a shard ID, shard-local rank,
     * and a specific role (e.g., shard member, shard leader, final committee member).
     */
    Node::Node(int globalRank)
        : globalRank(globalRank),
          shardId(-1),
          shardRank(-1),
          role(NodeRole::SHARD_MEMBER)
    {
    }

    /**
     * @brief Sets the shard-specific information and role for the node.
     * @param id The ID of the shard the node belongs to.
     * @param rank The rank of the node within its shard.
     * @param role The role of the node (e.g., SHARD_MEMBER, SHARD_LEADER).
     */
    void Node::setShardInfo(int id, int rank, NodeRole role)
    {
      shardId = id;
      shardRank = rank;
      this->role = role;
    }

    /**
     * @brief Retrieves the global MPI rank of the node.
     * @return The global MPI rank.
     */
    int Node::getGlobalRank() const
    {
      return globalRank;
    }

    /**
     * @brief Retrieves the shard ID the node belongs to.
     * @return The shard ID, or -1 if not assigned to a shard.
     */
    int Node::getShardId() const
    {
      return shardId;
    }

    /**
     * @brief Retrieves the rank of the node within its assigned shard.
     * @return The shard-local rank, or -1 if not assigned to a shard.
     */
    int Node::getShardRank() const
    {
      return shardRank;
    }

    /**
     * @brief Retrieves the role of the node in the network.
     * @return The NodeRole of the node.
     */
    NodeRole Node::getRole() const
    {
      return role;
    }

  } // namespace core
} // namespace sbmpi

```

# src/core/mempool/mempool.cpp

```cpp
/**
 * @file mempool.cpp
 * @brief Implements the Mempool class for managing pending transactions.
 */
#include "sbmpi/core/mempool/mempool.h"
#include <algorithm>
#include <vector>
#include "sbmpi/core/state/transaction.h"
namespace sbmpi
{
  namespace core
  {
    namespace mempool
    {

      /**
       * @brief Manages a pool of unconfirmed transactions.
       *
       * The Mempool is responsible for storing transactions that have been received
       * but not yet included in a block. It provides thread-safe operations for
       * adding, removing, and retrieving transactions.
       */
      Mempool::Mempool() {}

      /**
       * @brief Adds a transaction to the mempool.
       *
       * Prevents duplicate transactions based on their ID.
       * @param tx The transaction to add.
       * @return True if the transaction was added, false if it was a duplicate.
       */
      bool Mempool::add(const state::Transaction& tx)
      {
        std::lock_guard<std::mutex> lock(mtx);
        // Prevent duplicates
        auto it = std::find_if(
            transactions.begin(), transactions.end(),
            [&](const state::Transaction& t) { return t.id == tx.id; });
        if (it == transactions.end()) {
          transactions.push_back(tx);
          return true;
        }
        return false;
      }

      /**
       * @brief Removes a transaction from the mempool by its ID.
       * @param transactionId The ID of the transaction to remove.
       */
      void Mempool::remove(const std::string& transactionId)
      {
        std::lock_guard<std::mutex> lock(mtx);
        transactions.erase(
            std::remove_if(transactions.begin(), transactions.end(),
                           [&](const state::Transaction& tx) {
                             return tx.id == transactionId;
                           }),
            transactions.end());
      }

      /**
       * @brief Retrieves a specified number of transactions from the mempool.
       *
       * The retrieved transactions are removed from the mempool.
       * @param maxCount The maximum number of transactions to retrieve.
       * @return A vector of transactions.
       */
      std::vector<state::Transaction> Mempool::getTransactions(size_t maxCount)
      {
        std::lock_guard<std::mutex> lock(mtx);
        size_t count = std::min(maxCount, transactions.size());
        std::vector<state::Transaction> result(transactions.begin(),
                                               transactions.begin() + count);
        transactions.erase(transactions.begin(), transactions.begin() + count);
        return result;
      }

      /**
       * @brief Returns the current number of transactions in the mempool.
       * @return The size of the mempool.
       */
      size_t Mempool::size() const
      {
        std::lock_guard<std::mutex> lock(mtx);
        return transactions.size();
      }

    } // namespace mempool
  } // namespace core
} // namespace sbmpi

```

# src/core/blocks/blockheader.cpp

```cpp
/**
 * @file blockheader.cpp
 * @brief Implements the BlockHeader class for blockchain blocks.
 */
#include "../../../include/sbmpi/core/blocks/blockheader.h"
#include "../../../include/sbmpi/util/crypto.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Converts a byte array to its hexadecimal string representation.
       * @param data Pointer to the unsigned char array.
       * @param len The length of the byte array.
       * @return A std::string containing the hexadecimal representation.
       */
      static std::string toHex(const unsigned char* data, std::size_t len)
      {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (std::size_t i = 0; i < len; ++i)
          oss << std::setw(2) << static_cast<int>(data[i]);

        return oss.str();
      }

      /**
       * @brief Default constructor for BlockHeader.
       * Initializes height to 0, previousHash and merkleRoot to empty strings,
       * and timestamp to the current system time.
       */
      BlockHeader::BlockHeader()
          : height(0),
            previousHash(""),
            merkleRoot(""),
            timestamp(std::chrono::system_clock::now())
      {
      }

      /**
       * @brief Main constructor for BlockHeader.
       * @param height_ The height of the block in the blockchain.
       * @param previousHash_ The hash of the previous block.
       * @param merkleRoot_ The Merkle root of all transactions in the block.
       */
      BlockHeader::BlockHeader(int height_, const std::string& previousHash_,
                               const std::string& merkleRoot_)
          : height(height_),
            previousHash(previousHash_),
            merkleRoot(merkleRoot_),
            timestamp(std::chrono::system_clock::now())
      {
      }

      /**
       * @brief Calculates the SHA-256 cryptographic hash of the block header.
       *
       * The hash is computed by concatenating the block's height, previous hash,
       * Merkle root, and timestamp (in milliseconds) into a single string and
       * then applying SHA-256.
       * @return A std::string representing the SHA-256 hash of the block header.
       */
      std::string BlockHeader::hash() const
      {
        // Convert timestamp → integer (portable)
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                      timestamp.time_since_epoch())
                      .count();

        // Concatenate input (canonical serialization for hashing)
        std::ostringstream input;
        input << height << '|' << previousHash << '|' << merkleRoot << '|'
              << ts;

        const std::string data = input.str();
        return util::sha256(data);
      }

      /**
       * @brief Serializes the BlockHeader into a vector of characters.
       *
       * The format is:
       *   [height:int32]
       *   [timestamp:int64 ms]
       *   [prevHashLen:int32][prevHash bytes]
       *   [merkleLen:int32][merkle bytes]
       * @return A std::vector<char> containing the serialized block header data.
       */
      std::vector<char> BlockHeader::serialize() const
      {
        std::vector<char> buffer;

        auto appendInt32 = [&](int32_t v) {
          char b[4];
          std::memcpy(b, &v, 4);
          buffer.insert(buffer.end(), b, b + 4);
        };

        auto appendInt64 = [&](int64_t v) {
          char b[8];
          std::memcpy(b, &v, 8);
          buffer.insert(buffer.end(), b, b + 8);
        };

        auto appendString = [&](const std::string& s) {
          appendInt32(static_cast<int32_t>(s.size()));
          buffer.insert(buffer.end(), s.begin(), s.end());
        };

        // Serializable timestamp
        int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();

        appendInt32(height);
        appendInt64(ts);
        appendString(previousHash);
        appendString(merkleRoot);

        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a BlockHeader object.
       *
       * Reconstructs the BlockHeader from its serialized byte representation.
       * Throws std::runtime_error if the buffer is too small or data is malformed.
       * @param data The std::vector<char> containing the serialized block header data.
       */
      void BlockHeader::deserialize(const std::vector<char>& data)
      {
        if (data.size() < 4 + 8) {
          throw std::runtime_error(
              "BlockHeader::deserialize: buffer too small");
        }

        std::size_t offset = 0;

        auto readInt32 = [&](int32_t& out) {
          if (offset + 4 > data.size())
            throw std::runtime_error("BlockHeader::deserialize: out of range");
          std::memcpy(&out, &data[offset], 4);
          offset += 4;
        };

        auto readInt64 = [&](int64_t& out) {
          if (offset + 8 > data.size())
            throw std::runtime_error("BlockHeader::deserialize: out of range");
          std::memcpy(&out, &data[offset], 8);
          offset += 8;
        };

        auto readString = [&](std::string& s) {
          int32_t len = 0;
          readInt32(len);
          if (len < 0 || offset + len > data.size())
            throw std::runtime_error(
                "BlockHeader::deserialize: invalid string length");

          s.assign(&data[offset], len);
          offset += len;
        };

        int64_t tsMillis = 0;

        readInt32(height);
        readInt64(tsMillis);
        readString(previousHash);
        readString(merkleRoot);

        timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(tsMillis));
      }

    } // namespace blocks
  } // namespace core
} // namespace sbmpi
```

# src/core/blocks/micro_block.cpp

```cpp
/**
 * @file micro_block.cpp
 * @brief Implements the MicroBlock class, representing a block within a single shard.
 */
#include "../../../include/sbmpi/core/blocks/micro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Represents a MicroBlock, a block specific to a single shard.
       *
       * MicroBlocks contain transactions processed within a particular shard
       * and are eventually aggregated into MacroBlocks by the Final Committee.
       */
      MicroBlock::MicroBlock() : shardId(0) {}

      /**
       * @brief Constructs a MicroBlock with a specified shard ID.
       * @param shardId The identifier of the shard this MicroBlock belongs to.
       */
      MicroBlock::MicroBlock(int shardId) : shardId(shardId) {}

      /**
       * @brief Returns the type name of the block.
       * @return A std::string indicating the block type, "MicroBlock".
       */
      std::string MicroBlock::getType() const
      {
        return "MicroBlock";
      }

      /**
       * @brief Serializes the MicroBlock into a vector of characters.
       *
       * The serialization includes the block header, the shard ID, and all
       * transactions contained within this MicroBlock.
       * @return A std::vector<char> containing the serialized MicroBlock data.
       */
      std::vector<char> MicroBlock::serialize() const
      {
        std::vector<char> buffer;

        std::vector<char> headerData = header.serialize();
        util::pack(static_cast<int>(headerData.size()), buffer);
        buffer.insert(buffer.end(), headerData.begin(), headerData.end());

        util::pack(shardId, buffer);

        util::pack(static_cast<int>(transactions.size()), buffer);
        for (const auto& tx : transactions) {
          std::vector<char> txData = tx.serialize();
          util::pack(static_cast<int>(txData.size()), buffer);
          buffer.insert(buffer.end(), txData.begin(), txData.end());
        }

        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a MicroBlock object.
       *
       * Reconstructs the MicroBlock from its serialized byte representation,
       * including its header, shard ID, and transactions.
       * @param data The std::vector<char> containing the serialized MicroBlock data.
       */
      void MicroBlock::deserialize(const std::vector<char>& data)
      {
        int offset = 0;

        int headerSize = util::unpack_int(data, offset);
        std::vector<char> headerVec(data.begin() + offset,
                                    data.begin() + offset + headerSize);
        header.deserialize(headerVec);
        offset += headerSize;

        shardId = util::unpack_int(data, offset);

        transactions.clear();
        int numTransactions = util::unpack_int(data, offset);
        for (int i = 0; i < numTransactions; ++i) {
          int txSize = util::unpack_int(data, offset);
          std::vector<char> txData(data.begin() + offset,
                                    data.begin() + offset + txSize);
          state::Transaction tx;
          tx.deserialize(txData);
          transactions.push_back(tx);
          offset += txSize;
        }
      }

    } // namespace blocks
  } // namespace core
} // namespace sbmpi

```

# src/core/blocks/macro_block.cpp

```cpp
/**
 * @file macro_block.cpp
 * @brief Implements the MacroBlock class, which aggregates MicroBlocks.
 */
#include "../../../include/sbmpi/core/blocks/macro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Represents a MacroBlock, which is a block that aggregates multiple MicroBlocks.
       *
       * MacroBlocks are typically used in sharded blockchain architectures to finalize
       * the state of multiple shards. They contain references (hashes) to the
       * MicroBlocks they include, and can also contain their own transactions
       * (e.g., cross-shard transactions, rewards).
       */
      MacroBlock::MacroBlock() {}

      /**
       * @brief Returns the type name of the block.
       * @return A std::string indicating the block type, "MacroBlock".
       */
      std::string MacroBlock::getType() const
      {
        return "MacroBlock";
      }

      /**
       * @brief Adds the hash of a MicroBlock to this MacroBlock.
       * @param microBlock The MicroBlock whose hash is to be added.
       */
      void MacroBlock::addMicroBlock(const MicroBlock& microBlock)
      {
        microBlockHashes.push_back(microBlock.getHash());
      }

      /**
       * @brief Serializes the MacroBlock into a vector of characters.
       *
       * The serialization includes the block header, the hashes of all contained
       * MicroBlocks, and any transactions directly within this MacroBlock.
       * @return A std::vector<char> containing the serialized MacroBlock data.
       */
      std::vector<char> MacroBlock::serialize() const
      {
        std::vector<char> buffer;

        // Serialize header
        std::vector<char> headerData = header.serialize();
        util::pack(static_cast<int>(headerData.size()), buffer);
        buffer.insert(buffer.end(), headerData.begin(), headerData.end());

        // Serialize micro block hashes
        util::pack(static_cast<int>(microBlockHashes.size()), buffer);
        for (const auto& hash : microBlockHashes) {
          util::pack(hash, buffer);
        }

        // Macro blocks can also contain transactions (e.g. rewards, cross-shard
        // settlements)
        util::pack(static_cast<int>(transactions.size()), buffer);
        for (const auto& tx : transactions) {
          std::vector<char> txData = tx.serialize();
          util::pack(static_cast<int>(txData.size()), buffer);
          buffer.insert(buffer.end(), txData.begin(), txData.end());
        }

        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a MacroBlock object.
       *
       * Reconstructs the MacroBlock from its serialized byte representation,
       * including its header, micro block hashes, and transactions.
       * @param data The std::vector<char> containing the serialized MacroBlock data.
       */
      void MacroBlock::deserialize(const std::vector<char>& data)
      {
        int offset = 0;

        // Deserialize header
        int headerSize = util::unpack_int(data, offset);
        std::vector<char> headerVec(data.begin() + offset,
                                    data.begin() + offset + headerSize);
        header.deserialize(headerVec);
        offset += headerSize;

        // Deserialize micro block hashes
        microBlockHashes.clear();
        int numHashes = util::unpack_int(data, offset);
        for (int i = 0; i < numHashes; ++i) {
          microBlockHashes.push_back(util::unpack_string(data, offset));
        }

        // Deserialize transactions
        transactions.clear();
        int numTransactions = util::unpack_int(data, offset);
        for (int i = 0; i < numTransactions; ++i) {
          int txSize = util::unpack_int(data, offset);
          std::vector<char> txData(data.begin() + offset,
                                    data.begin() + offset + txSize);
          state::Transaction tx;
          tx.deserialize(txData);
          transactions.push_back(tx);
          offset += txSize;
        }
      }

    } // namespace blocks
  } // namespace core
} // namespace sbmpi

```

# src/core/blocks/block.cpp

```cpp
/**
 * @file block.cpp
 * @brief Implements the base Block class.
 */
#include "../../../include/sbmpi/core/blocks/block.h"
#include <string>
#include <vector>
#include "../../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      /**
       * @brief Retrieves the cryptographic hash of the block.
       *
       * This method delegates the hashing responsibility to the block's header.
       * @return A std::string representing the SHA256 hash of the block header.
       */
      std::string Block::getHash() const
      {
        return header.hash();
      }

    } // namespace blocks
  } // namespace core

} // namespace sbmpi
```

# src/core/state/state.cpp

```cpp
/**
 * @file state.cpp
 * @brief Implements the State class for managing the blockchain's global state.
 */
#include "../../../include/sbmpi/core/state/state.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Manages the global state of the blockchain, primarily account balances.
       *
       * This class provides functionality to apply transactions and query account balances.
       * It initializes with a genesis address having a predefined balance.
       */
      State::State()
      {
        // Maybe some initial state, e.g. for genesis
        balances["genesis_address"] = 1000000.0;
      }

      /**
       * @brief Applies a transaction to the current state, updating account balances.
       *
       * Performs basic validation: verifies the transaction's signature and checks
       * if the sender has sufficient funds. If valid, it debits the sender and
       * credits the receiver.
       * @param tx The transaction to apply.
       * @return True if the transaction was successfully applied, false otherwise (e.g., invalid signature, insufficient funds).
       */
      bool State::apply(const Transaction& tx)
      {
        if (!tx.verify()) {
          return false;
        }

        auto from_it = balances.find(tx.from);
        if (from_it == balances.end() || from_it->second < tx.amount) {
          // Sender does not exist or has insufficient funds
          return false;
        }

        from_it->second -= tx.amount;

        auto to_it = balances.find(tx.to);
        if (to_it == balances.end()) {
          balances[tx.to] = tx.amount;
        } else {
          to_it->second += tx.amount;
        }

        return true;
      }

      /**
       * @brief Retrieves the balance of a given address.
       * @param address The address whose balance is to be retrieved.
       * @return The balance of the address, or 0.0 if the address does not exist in the state.
       */
      double State::getBalance(const std::string& address) const
      {
        auto it = balances.find(address);
        if (it != balances.end()) {
          return it->second;
        }
        return 0.0;
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

```

# src/core/state/transaction.cpp

```cpp
/**
 * @file transaction.cpp
 * @brief Implements the Transaction class for representing blockchain transactions.
 */
#include "../../../include/sbmpi/core/state/transaction.h"

#include <chrono>
#include <sstream>
#include <vector>

#include "../../../include/sbmpi/util/crypto.h"
#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Represents a single transaction in the blockchain.
       *
       * A transaction includes sender, receiver, amount, a unique ID, and a digital signature.
       * It provides methods for signing, verifying, serializing, and deserializing.
       */
      Transaction::Transaction() : amount(0.0) {}

      /**
       * @brief Constructs a new Transaction object.
       *
       * Generates a unique transaction ID based on sender, receiver, amount, and timestamp.
       * @param from The sender's address.
       * @param to The receiver's address.
       * @param amount The amount of currency to transfer.
       */
      Transaction::Transaction(const std::string& from, const std::string& to,
                               double amount)
          : from(from), to(to), amount(amount)
      {
        std::stringstream ss;
        ss << from << to << amount
           << std::chrono::system_clock::now().time_since_epoch().count();
        id = util::sha256(ss.str());
      }

      /**
       * @brief Signs the transaction using a provided private key.
       *
       * The signature is a hash of the transaction data combined with the private key.
       * (Note: This is a simplified simulation of signing for demonstration purposes).
       * @param privateKey The private key used to sign the transaction.
       */
      void Transaction::sign(const std::string& privateKey)
      {
        std::string data = from + to + std::to_string(amount);
        signature = util::sign(data, privateKey);
      }

      /**
       * @brief Verifies the transaction's signature.
       *
       * Checks if the signature is valid for the transaction data and the sender's public key.
       * (Note: This verification is simplified for simulation and assumes 'from' is the public key).
       * @return True if the signature is valid, false otherwise.
       */
      bool Transaction::verify() const
      {
        std::string data = from + to + std::to_string(amount);
        // Assumes the 'from' address is the public key for dummy verification
        return util::verify(data, signature, from);
      }

      /**
       * @brief Serializes the Transaction object into a vector of characters.
       *
       * The serialization includes the transaction ID, sender, receiver, amount, and signature.
       * @return A std::vector<char> containing the serialized transaction data.
       */
      std::vector<char> Transaction::serialize() const
      {
        std::vector<char> buffer;
        util::pack(id, buffer);
        util::pack(from, buffer);
        util::pack(to, buffer);
        util::pack(amount, buffer);
        util::pack(signature, buffer);
        return buffer;
      }

      /**
       * @brief Deserializes a vector of characters into a Transaction object.
       *
       * Reconstructs the Transaction from its serialized byte representation.
       * @param data The std::vector<char> containing the serialized transaction data.
       */
      void Transaction::deserialize(const std::vector<char>& data)
      {
        int offset = 0;
        id = util::unpack_string(data, offset);
        from = util::unpack_string(data, offset);
        to = util::unpack_string(data, offset);
        amount = util::unpack_double(data, offset);
        signature = util::unpack_string(data, offset);
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

```

# src/core/state/genesis.cpp

```cpp
/**
 * @file genesis.cpp
 * @brief Implements the creation of the genesis block.
 */
#include "../../../include/sbmpi/core/state/genesis.h"
#include <memory>
#include "../../../include/sbmpi/core/blocks/macro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      /**
       * @brief Creates a unique pointer to a new genesis MacroBlock.
       *
       * The genesis block is the first block in the blockchain, initialized with
       * a height of 0, a "0" previous hash, a "0" Merkle root (as there are no
       * initial transactions to hash), and the current timestamp.
       * @return A unique_ptr to the newly created MacroBlock representing the genesis block.
       */
      std::unique_ptr<sbmpi::core::blocks::MacroBlock> createGenesisBlock()
      {
        auto genesisBlock = std::make_unique<sbmpi::core::blocks::MacroBlock>();
        genesisBlock->header.height = 0;
        genesisBlock->header.previousHash = "0";
        // In a real scenario, merkle root would be calculated from genesis
        // transactions
        genesisBlock->header.merkleRoot = "0";
        genesisBlock->header.timestamp = std::chrono::system_clock::now();
        return genesisBlock;
      }

    } // namespace state
  } // namespace core
} // namespace sbmpi

```

# src/util/generator.cpp

```cpp
/**
 * @file generator.cpp
 * @brief Implements utility functions for generating mock data, specifically transactions.
 */
#include "../../include/sbmpi/util/generator.h"
#include <algorithm>  // For std::min
#include <iomanip>    // For std::setw, std::fixed, std::setprecision
#include <iostream>
#include <random>
#include <string>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Generates a specified number of mock transactions.
     *
     * Transactions are generated with random senders, receivers (ensuring different
     * from sender), and amounts. A fixed seed is used for the random number
     * generator to ensure deterministic and repeatable results for benchmarking.
     * Each transaction is assigned a unique, deterministic ID and a simulated signature.
     * The first 10 generated transactions are pretty-printed to stdout.
     *
     * @param count The number of mock transactions to generate.
     * @return A std::vector of generated core::state::Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count)
    {
      std::vector<sbmpi::core::state::Transaction> transactions;
      transactions.reserve(count);

      // CRITICAL: Use a fixed seed (42) for deterministic, repeatable results.
      // This ensures consistent benchmarking between serial and parallel runs.
      std::mt19937 gen(42);

      // Simulate a pool of 10,000 unique users
      std::uniform_int_distribution<> userDist(1, 10000);
      // Random transaction amounts between 0.01 and 1000.00
      std::uniform_real_distribution<> amountDist(0.01, 1000.0);

      for (size_t i = 0; i < count; ++i) {
        // Generate Sender
        std::string sender = "user_" + std::to_string(userDist(gen));

        // Generate Receiver (ensure it is different from sender)
        std::string receiver;
        do {
          receiver = "user_" + std::to_string(userDist(gen));
        } while (sender == receiver);

        double amount = amountDist(gen);

        // Instantiate the Transaction
        sbmpi::core::state::Transaction tx(sender, receiver, amount);

        // Assign a unique, deterministic ID
        tx.id = std::to_string(i);

        // Sign the transaction
        // Note: The signature here is simulated for the purpose of the mock
        // data.
        tx.sign("private_key_" + sender);

        transactions.push_back(tx);
      }

      // --- NEW LOGIC: Pretty Print First 10 Transactions ---
      size_t transactionsToPrint = std::min(count, (size_t)10);

      if (transactionsToPrint > 0) {
        std::cout << "\n--- Generated Mock Transactions (First "
                  << transactionsToPrint << " of " << count << ") ---"
                  << std::endl;
        std::cout << "ID       | FROM (User)  | TO (User)    | AMOUNT"
                  << std::endl;
        std::cout << "--------------------------------------------------------"
                  << std::endl;

        // Iterate only through the first N transactions
        for (size_t i = 0; i < transactionsToPrint; ++i) {
          const auto& tx = transactions[i];

          // Output format: ID | FROM | TO | AMOUNT
          std::cout << std::left << std::setw(8) << tx.id << "| " << std::left
                    << std::setw(12) << tx.from << "| " << std::left
                    << std::setw(12) << tx.to << "| $" << std::fixed
                    << std::setprecision(2) << tx.amount << std::endl;
        }
        std::cout
            << "--------------------------------------------------------\n"
            << std::endl;
      }

      return transactions;
    }

  } // namespace util
} // namespace sbmpi
// #include "../../include/sbmpi/util/generator.h"
// #include <iostream>
// #include <random>
// #include <string>

// namespace sbmpi
// {
//   namespace util
//   {

//     std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
//         size_t count)
//     {
//       std::vector<sbmpi::core::state::Transaction> transactions;
//       transactions.reserve(count);

//       // CRITICAL: Use a fixed seed (42) for deterministic, repeatable
//       results.
//       // This ensures consistent benchmarking between serial and parallel
//       runs. std::mt19937 gen(42);

//       // Simulate a pool of 10,000 unique users
//       std::uniform_int_distribution<> userDist(1, 10000);
//       // Random transaction amounts between 0.01 and 1000.00
//       std::uniform_real_distribution<> amountDist(0.01, 1000.0);

//       for (size_t i = 0; i < count; ++i) {
//         // Generate Sender
//         std::string sender = "user_" + std::to_string(userDist(gen));

//         // Generate Receiver (ensure it is different from sender)
//         std::string receiver;
//         do {
//           receiver = "user_" + std::to_string(userDist(gen));
//         } while (sender == receiver);

//         double amount = amountDist(gen);

//         // Instantiate the Transaction
//         // Note: The constructor provided in transaction.h handles from, to,
//         and
//         // amount.
//         sbmpi::core::state::Transaction tx(sender, receiver, amount);

//         // Assign a unique, deterministic ID
//         tx.id = std::to_string(i);

//         // Sign the transaction
//         // In a real app, we would look up the user's specific private key.
//         // For simulation, we generate a consistent dummy key string based on
//         // the sender.
//         tx.sign("private_key_" + sender);

//         transactions.push_back(tx);
//       }

//       return transactions;
//     }

//   }  // namespace util
// }  // namespace sbmpi
```

# src/util/timer.cpp

```cpp
/**
 * @file timer.cpp
 * @brief Implements the Timer class for measuring elapsed time with high precision.
 */
#include "../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief A utility class for measuring elapsed time with high resolution.
     *
     * Provides methods to start, stop, and retrieve the elapsed time in
     * seconds or milliseconds.
     */
    // Default constructor is implicitly defined and sufficient.

    /**
     * @brief Starts the timer, recording the current high-resolution time point.
     */
    void Timer::start()
    {
      startTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Stops the timer, recording the current high-resolution time point.
     */
    void Timer::stop()
    {
      endTime = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Calculates the elapsed time between start() and stop() in seconds.
     * @return The elapsed time in double-precision seconds.
     */
    double Timer::elapsedSeconds() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double>>(
                 endTime - startTime)
          .count();
    }

    /**
     * @brief Calculates the elapsed time between start() and stop() in milliseconds.
     * @return The elapsed time in double-precision milliseconds.
     */
    double Timer::elapsedMilliseconds() const
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                   startTime)
          .count();
    }

  } // namespace util
} // namespace sbmpi

```

# src/util/crypto.cpp

```cpp
/**
 * @file crypto.cpp
 * @brief Provides cryptographic utility functions for hashing, signing, and verifying.
 *
 * This file implements SHA-256 hashing using OpenSSL, and dummy implementations
 * for digital signing and verification for simulation purposes. It also includes
 * a Merkle tree root calculation function.
 */
#include "../../include/sbmpi/util/crypto.h"
#include <openssl/evp.h>  // Use EVP header instead of sha.h
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Computes the SHA-256 hash of a given string.
     * @param data The input string to be hashed.
     * @return A std::string representing the hexadecimal SHA-256 hash.
     */
    std::string sha256(const std::string& data)
    {
      unsigned char hash[EVP_MAX_MD_SIZE];
      unsigned int  lengthOfHash = 0;

      EVP_MD_CTX* context = EVP_MD_CTX_new();

      if (context != nullptr) {
        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr)) {
          if (EVP_DigestUpdate(context, data.c_str(), data.size())) {
            EVP_DigestFinal_ex(context, hash, &lengthOfHash);
          }
        }
        EVP_MD_CTX_free(context);
      }

      std::stringstream ss;
      for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
      }
      return ss.str();
    }

    /**
     * @brief Generates a dummy digital signature for given data using a private key.
     *
     * For simulation purposes, the signature is simply the SHA-256 hash of
     * the concatenated data and private key. In a real system, this would
     * involve asymmetric cryptography.
     * @param data The data to be signed.
     * @param privateKey The private key used for signing.
     * @return A std::string representing the dummy signature.
     */
    std::string sign(const std::string& data, const std::string& privateKey)
    {
      // Dummy implementation: Signature = SHA256(Data + PrivateKey)
      return sha256(data + privateKey);
    }

    /**
     * @brief Verifies a dummy digital signature against data and a public key.
     *
     * For simulation purposes, this checks if the provided signature matches
     * the SHA-256 hash of the data concatenated with a reconstructed "private key"
     * based on the public key (as used by the mock generator).
     * @param data The original data that was signed.
     * @param signature The signature to verify.
     * @param publicKey The public key (in this simulation, the sender's address).
     * @return True if the signature is valid, false otherwise.
     */
    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey)
    {
      // Align verification with the generator's signing logic.
      // The generator signs with "private_key_" + sender.
      // The transaction passes 'sender' (the address) as 'publicKey'.
      // So to verify, we must reconstruct the signing key used by the mock
      // generator.

      // In a real system, we would mathematically verify (Sig, Data, PubKey).
      // In this simulation, we check: Is Sig == SHA256(Data + "private_key_" +
      // PubKey)?
      std::string expectedPrivateKey = "private_key_" + publicKey;

      return signature == sha256(data + expectedPrivateKey);
    }

    /**
     * @brief Calculates the Merkle root hash given a vector of 'Transactions'.
     *
     * This function constructs a Merkle tree from the transaction IDs (hashes)
     * and returns the root hash. If the number of transactions is odd, the last
     * hash is duplicated at each level.
     * @param transactions A reference to a std::vector containing
     * core::state::Transaction instances.
     * @return A std::string representation of the calculated Merkle root, or an
     *         empty string if the input vector is empty.
     */
    std::string merkle(
        const std::vector<core::state::Transaction>& transactions)
    {
      // Cannot compute merkle root with empty set of TXs
      if (transactions.empty()) {
        return "";
      }

      // Create a vector containing the IDs (hashes) of the TXs
      std::vector<std::string> currentTransactions;
      for (const auto& tx : transactions) {
        currentTransactions.push_back(tx.id);
      }

      // Combine the hashes until the merkle root is reached
      while (currentTransactions.size() != 1) {
        // Create a new vector containing the combined hashes
        std::vector<std::string> newTransactions;
        // Iterate the current vector of transactions by steps of 2
        for (size_t i = 0; i < currentTransactions.size(); i += 2) {
          // If two hashes can be accessed, then hash the combination of the two
          // neighboring hashes
          if (i + 1 < currentTransactions.size()) {
            std::string newHash =
                sha256(currentTransactions[i] + currentTransactions[i + 1]);
            newTransactions.push_back(newHash);
          }
          // Otherwise, add the single edge hash to the new vector of hashes
          else {
            newTransactions.push_back(currentTransactions[i]);
          }
        }
        // Reference the new vector at the end of iterations
        currentTransactions = newTransactions;
      }

      // Return the merkle root
      return currentTransactions[0];
    }

  } // namespace util
} // namespace sbmpi

```

# src/util/logging.cpp

```cpp
/**
 * @file logging.cpp
 * @brief Implements the Logger class for structured and level-based logging.
 */
#include "../../include/sbmpi/util/logging.h"
#include <iostream>
#include <mutex>
#include "sbmpi/util/errors.h"

namespace sbmpi
{
  namespace util
  {

    // Definition of static member variable
    LogLevel Logger::loggerLevel = LogLevel::INFO;

    // Mutex to prevent garbled output if multiple threads log simultaneously
    // (Note: MPI processes are separate, but threads within a process need
    // this)
    static std::mutex log_mutex;

    /**
     * @brief Configures the logger with the MPI rank of the current process.
     *
     * This rank is prepended to log messages to identify the source.
     * @param r The MPI rank of the process.
     */
    void Logger::configure(int r)
    {
      this->rank = r;
    }

    /**
     * @brief Sets the minimum logging level.
     *
     * Messages with a level equal to or higher than the set level will be logged.
     * @param level The new minimum LogLevel.
     */
    void Logger::setLevel(LogLevel level)
    {
      loggerLevel = level;
    }

    /**
     * @brief Logs a message with a specified level.
     *
     * Messages are printed to standard output, prefixed with the MPI rank and
     * the log level string. Thread-safe due to a mutex.
     * @param level The LogLevel of the message.
     * @param message The string content of the log message.
     */
    void Logger::log(LogLevel level, const std::string& message)
    {
      if (level <= loggerLevel) {
        std::lock_guard<std::mutex> lock(log_mutex);

        std::string levelStr;
        switch (level) {
          case LogLevel::ERROR:
            levelStr = "[ERROR]";
            break;
          case LogLevel::INFO:
            levelStr = "[INFO] ";
            break;
          case LogLevel::DEBUG:
            levelStr = "[DEBUG]";
            break;
          case LogLevel::FATAL:
            levelStr = "[FATAL]";
            break;
          default:
            return;
        }

        // Format: [Rank 0] [INFO] Message content
        std::cout << "[Rank " << rank << "] " << levelStr << " " << message
                  << std::endl;
      }
    }

    /**
     * @brief Logs an informational message.
     * @param message The string content of the informational message.
     */
    void Logger::info(const std::string& message)
    {
      log(LogLevel::INFO, message);
    }

    /**
     * @brief Logs an error message.
     * @param message The string content of the error message.
     */
    void Logger::error(const std::string& message)
    {
      log(LogLevel::ERROR, message);
    }

    /**
     * @brief Logs a debug message.
     * @param message The string content of the debug message.
     */
    void Logger::debug(const std::string& message)
    {
      log(LogLevel::DEBUG, message);
    }

    /**
     * @brief Logs a fatal error message and terminates the program.
     *
     * This function also calls the global `sbmpi::util::fatal` function to
     * ensure program termination with the specified error code.
     * @param code The ErrorCode associated with the fatal error.
     * @param message The string content of the fatal error message.
     */
    void Logger::fatal(ErrorCode code, const std::string& message)
    {
      log(LogLevel::FATAL, message);
      // CRITICAL: Also call the global fatal function to terminate the program.
      // The global fatal function is responsible for printing to stderr and exiting.
      sbmpi::util::fatal(code, message);
    }

  } // namespace util
} // namespace sbmpi

```

# src/util/metrics.cpp

```cpp
/**
 * @file metrics.cpp
 * @brief Implements the Metrics class for recording and reporting simulation performance.
 */
#include "../../include/sbmpi/util/metrics.h"
#include <fstream>
#include <iostream>
#include <map>

namespace sbmpi
{
  namespace util
  {

    namespace
    {
      /**
       * @brief Structure to hold results for a single experiment.
       */
      struct ExperimentResult {
        std::string name;          ///< Name of the experiment.
        double      totalTime;     ///< Total time taken for the experiment.
        int         numTransactions; ///< Number of transactions processed.
      };
      std::map<std::string, ExperimentResult> results; ///< Stores all experiment results.
    }  // namespace

    /**
     * @brief The Metrics class provides static methods to record and save simulation performance metrics.
     *
     * It tracks total time and number of transactions for various experiments
     * and can calculate throughput, saving all data to a CSV file.
     */
    // No explicit constructor/destructor needed for a static utility class.

    /**
     * @brief Records the time and transaction count for a specific experiment.
     * @param experimentName A unique name for the experiment.
     * @param totalTime The total time measured for the experiment in seconds.
     * @param numTransactions The number of transactions processed during the experiment.
     */
    void Metrics::recordTime(const std::string& experimentName,
                             double totalTime, int numTransactions)
    {
      results[experimentName] = {experimentName, totalTime, numTransactions};
    }

    /**
     * @brief Calculates the throughput (transactions per second) for an experiment.
     * @param totalTime The total time taken for the experiment.
     * @param numTransactions The number of transactions processed.
     * @return The calculated throughput, or 0 if totalTime is zero.
     */
    double Metrics::calculateThroughput(double totalTime, int numTransactions)
    {
      if (totalTime == 0) return 0;
      return numTransactions / totalTime;
    }

    /**
     * @brief Saves all recorded metrics to a CSV file.
     *
     * The CSV file will contain columns for Experiment Name, Total Time,
     * Number of Transactions, and Calculated Throughput.
     * @param filepath The path to the output CSV file.
     */
    void Metrics::save(const std::string& filepath)
    {
      std::ofstream file(filepath);
      if (!file.is_open()) {
        std::cerr << "Failed to open metrics file: " << filepath << std::endl;
        return;
      }
      file << "Experiment,TotalTime,NumTransactions,Throughput" << std::endl;
      for (const auto& pair : results) {
        const auto& result = pair.second;
        double      throughput =
            calculateThroughput(result.totalTime, result.numTransactions);
        file << result.name << "," << result.totalTime << ","
             << result.numTransactions << "," << throughput << std::endl;
      }
    }

  }  // namespace util
}  // namespace sbmpi

```

# src/util/errors.cpp

```cpp
/**
 * @file errors.cpp
 * @brief Implements utility functions for error handling and program termination.
 */
#include "../../include/sbmpi/util/errors.h"
#include <cstdlib>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Logs a fatal error message to stderr and terminates the program.
     * @param code The ErrorCode representing the type of error.
     * @param message A descriptive error message.
     */
    void fatal(ErrorCode code, const std::string& message)
    {
      std::cerr << "Fatal Error [" << static_cast<int>(code) << "]: " << message
                << std::endl;
      exit(static_cast<int>(code));
    }

  } // namespace util
} // namespace sbmpi

```

# src/util/serialization.cpp

```cpp
/**
 * @file serialization.cpp
 * @brief Provides utility functions for serializing and deserializing primitive types and strings into a byte buffer.
 *
 * These functions are essential for converting structured data into a format
 * suitable for network transmission or storage, and for reconstructing it back.
 */
#include "../../include/sbmpi/util/serialization.h"
#include <cstring>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Packs an integer value into a character vector buffer.
     * @param value The integer to pack.
     * @param buffer The std::vector<char> buffer to append the packed integer to.
     */
    void pack(int value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int));
    }

    /**
     * @brief Packs a double-precision floating-point value into a character vector buffer.
     * @param value The double to pack.
     * @param buffer The std::vector<char> buffer to append the packed double to.
     */
    void pack(double value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(double));
    }

    /**
     * @brief Packs a string value into a character vector buffer.
     *
     * The string's length is packed first as an integer, followed by the string's characters.
     * @param value The string to pack.
     * @param buffer The std::vector<char> buffer to append the packed string to.
     */
    void pack(const std::string& value, std::vector<char>& buffer)
    {
      int len = value.length();
      pack(len, buffer);
      buffer.insert(buffer.end(), value.begin(), value.end());
    }

    /**
     * @brief Unpacks an integer value from a character vector buffer.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked integer value.
     */
    int unpack_int(const std::vector<char>& buffer, int& offset)
    {
      int value;
      std::memcpy(&value, buffer.data() + offset, sizeof(int));
      offset += sizeof(int);
      return value;
    }

    /**
     * @brief Unpacks a double-precision floating-point value from a character vector buffer.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked double value.
     */
    double unpack_double(const std::vector<char>& buffer, int& offset)
    {
      double value;
      std::memcpy(&value, buffer.data() + offset, sizeof(double));
      offset += sizeof(double);
      return value;
    }

    /**
     * @brief Unpacks a string value from a character vector buffer.
     *
     * Reads the string's length first, then extracts the characters.
     * @param buffer The std::vector<char> buffer to unpack from.
     * @param offset A reference to the current offset in the buffer, which will be updated.
     * @return The unpacked string value.
     */
    std::string unpack_string(const std::vector<char>& buffer, int& offset)
    {
      int len = unpack_int(buffer, offset);
      std::string value(buffer.data() + offset, len);
      offset += len;
      return value;
    }

  } // namespace util
} // namespace sbmpi

```

# src/util/config.cpp

```cpp
/**
 * @file config.cpp
 * @brief Implements the Config class for parsing command-line arguments and managing simulation settings.
 */
#include "../../include/sbmpi/util/config.h"
#include <iostream>
#include <string>

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Manages configuration settings for the simulation, typically parsed from command-line arguments.
     *
     * This class provides methods to parse arguments and print the current configuration.
     */
    // Assuming the definition of Config::parse is in config.cpp
    // and the declarations of numNodes, numShards, numTransactions, verbose
    // are in config.h

    /**
     * @brief Parses command-line arguments to configure simulation parameters.
     *
     * Supports short and long flags for number of shards (-s, --shards) and
     * number of transactions (-t, --transactions). Also includes placeholders
     * for numNodes and verbose settings.
     * @param argc The number of command-line arguments.
     * @param argv An array of command-line argument strings.
     * @return True if arguments were parsed successfully, false otherwise.
     */
    bool Config::parse(int argc, char** argv)
    {
      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // ---  Handle Long Flags from Makefile ---
        if ((arg == "-s" || arg == "--shards") && i + 1 < argc) {
          // Argument is the number of shards
          numShards = std::stoi(argv[++i]);

        } else if ((arg == "-t" || arg == "--transactions") && i + 1 < argc) {
          // Argument is the number of transactions
          numTransactions = std::stoi(argv[++i]);

          // Keep existing rules for completeness, though unused by the current
          // Makefile ARGS
        } else if (arg == "-n" && i + 1 < argc) {
          numNodes = std::stoi(argv[++i]);
        } else if (arg == "-v" && i + 1 < argc) {
          verbose = std::stoi(argv[++i]);

        } else {
          // If the argument is none of the above, it is unknown
          std::cerr << "Unknown argument: " << arg << std::endl;
          return false;
        }
      }
      return true;
    }

    /**
     * @brief Prints the current configuration settings to standard output.
     */
    void Config::print() const
    {
      std::cout << "Configuration:" << std::endl;
      std::cout << "  Nodes: " << numNodes << std::endl;
      std::cout << "  Shards: " << numShards << std::endl;
      std::cout << "  Transactions: " << numTransactions << std::endl;
      std::cout << "  Verbose: " << verbose << std::endl;
    }

  } // namespace util
} // namespace sbmpi
```

# src/util/threadpool.cpp

```cpp

```

# src/network/cross_shard.cpp

```cpp
/**
 * @file cross_shard.cpp
 * @brief Provides placeholder functions for cross-shard transaction handling.
 *
 * In a real sharded blockchain, these functions would implement the logic
 * for identifying and routing transactions between different shards.
 */
#include "../../include/sbmpi/network/cross_shard.h"

#include "../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace network
  {

    /**
     * @brief Determines if a given transaction is a cross-shard transaction.
     *
     * This is currently a placeholder and always returns false, assuming all
     * transactions are intra-shard for the simulation's current scope.
     * In a full implementation, it would check if sender and receiver addresses
     * belong to different shards.
     * @param tx The transaction to check.
     * @return Always returns false in this simulation.
     */
    bool isCrossShard(const core::state::Transaction& tx)
    {
      // This is a placeholder. In a real system, this would involve checking
      // if the sender and receiver addresses belong to different shards.
      // For now, let's assume all transactions are intra-shard.
      return false;
    }

    /**
     * @brief Processes a cross-shard transaction.
     *
     * This is currently a placeholder function. In a real system, it would
     * contain the logic to route the transaction to the appropriate shard
     * and handle its execution across shard boundaries.
     * @param tx The cross-shard transaction to process.
     */
    void processTransaction(const core::state::Transaction& tx)
    {
      // This is a placeholder. In a real system, cross-shard transactions
      // would be routed to the appropriate shard.
    }

  } // namespace network
} // namespace sbmpi

```

# src/network/mpi_wrapper.cpp

```cpp
/**
 * @file mpi_wrapper.cpp
 * @brief Provides a simplified wrapper around common MPI communication functions.
 *
 * These functions facilitate sending, receiving, and broadcasting serialized
 * data (as std::vector<char>) between MPI processes.
 */
#include "../../include/sbmpi/network/mpi_wrapper.h"

#include <vector>

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    /**
     * @brief Sends a vector of characters to a specified destination MPI process.
     * @param data The std::vector<char> containing the data to send.
     * @param dest The rank of the destination MPI process.
     * @param tag The message tag.
     * @param comm The MPI communicator to use for sending.
     */
    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm)
    {
      MPI_Send(data.data(), static_cast<int>(data.size()), MPI_CHAR, dest, tag,
               comm);
    }

    /**
     * @brief Receives a vector of characters from a specified source MPI process.
     *
     * Uses MPI_Probe to determine the incoming message size before allocating
     * a buffer and receiving the data.
     * @param source The rank of the source MPI process (or MPI_ANY_SOURCE).
     * @param tag The message tag (or MPI_ANY_TAG).
     * @param comm The MPI communicator to use for receiving.
     * @return A std::vector<char> containing the received data.
     */
    std::vector<char> recv(int source, int tag, MPI_Comm comm)
    {
      MPI_Status status;
      MPI_Probe(source, tag, comm, &status);

      int size;
      MPI_Get_count(&status, MPI_CHAR, &size);

      std::vector<char> buffer(size);
      MPI_Recv(buffer.data(), size, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG,
               comm, MPI_STATUS_IGNORE);

      return buffer;
    }

    /**
     * @brief Broadcasts a vector of characters from a root MPI process to all
     *        other processes in the communicator.
     *
     * The root process sends the size of the data, then the data itself.
     * Non-root processes receive the size, resize their buffer, and then receive the data.
     * @param data A reference to the std::vector<char> to be broadcast (input for root, output for others).
     * @param root The rank of the root MPI process.
     * @param comm The MPI communicator to use for broadcasting.
     */
    void bcast(std::vector<char>& data, int root, MPI_Comm comm)
    {
      int rank;
      MPI_Comm_rank(comm, &rank);

      int size = static_cast<int>(data.size());
      MPI_Bcast(&size, 1, MPI_INT, root, comm);

      if (rank != root) {
        data.resize(size);
      }

      MPI_Bcast(data.data(), size, MPI_CHAR, root, comm);
    }

  } // namespace network
} // namespace sbmpi

```

# src/network/shard.cpp

```cpp
/**
 * @file shard.cpp
 * @brief Implements the Shard class, representing a single shard in the blockchain network.
 */
#include "../../include/sbmpi/network/shard.h"

#include <iostream>
#include <vector>

#include "../../include/sbmpi/consensus/pbft.h"
#include "../../include/sbmpi/core/blocks/micro_block.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/logging.h"
#include "../../include/sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    /**
     * @brief Represents a single shard in the sharded blockchain network.
     *
     * Each shard manages its own set of transactions, runs a PBFT consensus
     * protocol to produce microblocks, and reports these microblocks to the
     * Final Committee.
     */
    Shard::Shard(int id, MPI_Comm comm, int leaderRank)
        : id(id), communicator(comm), leaderRank(leaderRank)
    {
    }

    /**
     * @brief Destructor for the Shard class.
     */
    Shard::~Shard() {}

    /**
     * @brief Adds a transaction to the shard's local mempool.
     * @param tx The transaction to add.
     */
    void Shard::addTransaction(const core::state::Transaction& tx)
    {
      mempool.push_back(tx);
    }

    /**
     * @brief Returns the MPI communicator associated with this shard.
     * @return The MPI_Comm object for the shard.
     */
    MPI_Comm Shard::getCommunicator() const
    {
      return communicator;
    }

    /**
     * @brief Runs the PBFT consensus protocol within the shard.
     *
     * This method orchestrates the ingestion of transactions (for the shard leader),
     * the execution of the PBFT algorithm to agree on a microblock, and the
     * reporting of the finalized microblock to the Final Committee (by the shard leader).
     * @return The MicroBlock that has been agreed upon by the shard.
     */
    core::blocks::MicroBlock Shard::runConsensus()
    {
      int my_shard_rank;
      int shard_size;
      MPI_Comm_rank(communicator, &my_shard_rank);
      MPI_Comm_size(communicator, &shard_size);

      // 1. INGESTION PHASE
      // If I am the Shard Leader (Rank 0 in this shard), I must receive the
      // workload from Root (Rank 0 in World)
      if (my_shard_rank == 0) {
        // Receive serialized transactions from Root Process (Global Rank 0) via
        // COMM_WORLD Note: We use MPI_COMM_WORLD because Root is likely not in
        // our shard communicator
        std::vector<char> buffer = network::recv(0, 0, MPI_COMM_WORLD);

        int offset = 0;
        int numTx = util::unpack_int(buffer, offset);

        mempool.clear();
        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) + " Leader: Starting ingestion of " +
            std::to_string(numTx) + " transactions.");

        for (int i = 0; i < numTx; ++i) {
          int txSize = util::unpack_int(buffer, offset);
          std::vector<char> txData(buffer.begin() + offset,
                                          buffer.begin() + offset + txSize);
          core::state::Transaction tx;
          tx.deserialize(txData);

          // [LOG] Deep validation log
          if (tx.verify()) {
            util::Logger::getLogger().debug("Shard " + std::to_string(id) +
                                            ": Validated transaction " + tx.id);
            mempool.push_back(tx);
          } else {
            util::Logger::getLogger().error(
                "Shard " + std::to_string(id) +
                ": FAILED validation for transaction " + tx.id);
          }

          offset += txSize;
        }

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader: Ingestion complete. Mempool size: " +
            std::to_string(mempool.size()));
      }

      // 2. CONSENSUS PHASE
      // Instantiate PBFT engine.
      // Intra-shard leader is always Rank 0 of 'communicator'.
      consensus::PBFT pbft(communicator, my_shard_rank, 0, shard_size);

      // Run consensus. Only the leader passes the mempool; replicas pass empty
      // vectors (PBFT handles sync)
      std::string previousBlockHash =
          "0000000000000000000000000000000000000000000000000000000000000000";

      util::Logger::getLogger().info("Shard " + std::to_string(id) +
                                     ": Starting PBFT consensus.");

      // Pass previousBlockHash to run()
      core::blocks::MicroBlock microBlock =
          pbft.run(mempool, previousBlockHash);
      microBlock.shardId = id;  // Ensure block is tagged with our ID

      util::Logger::getLogger().info(
          "Shard " + std::to_string(id) +
          ": Consensus reached. MicroBlock Hash: " + microBlock.getHash());

      // 3. REPORTING PHASE
      // If I am the Shard Leader, send the valid MicroBlock to the Final
      // Committee Leader
      if (my_shard_rank == 0) {
        std::vector<char> serializedBlock = microBlock.serialize();

        // leaderRank member variable holds the FC Leader's Global Rank
        network::send(serializedBlock, leaderRank, 0, MPI_COMM_WORLD);

        util::Logger::getLogger().info(
            "Shard " + std::to_string(id) +
            " Leader sent MicroBlock to Final Committee (Rank " +
            std::to_string(leaderRank) + ").");
      }

      return microBlock;
    }

    /**
     * @brief Returns the unique identifier of this shard.
     * @return The shard ID.
     */
    int Shard::getId() const
    {
      return id;
    }

  } // namespace network
} // namespace sbmpi
```

# src/network/committee/final_committee.cpp

```cpp
/**
 * @file final_committee.cpp
 * @brief Implements the FinalCommittee class responsible for aggregating microblocks into macroblocks.
 */
#include "../../../include/sbmpi/network/committee/final_committee.h"

#include <iostream>
#include <vector>

#include "../../../include/sbmpi/core/blocks/macro_block.h"
#include "../../../include/sbmpi/core/blocks/micro_block.h"
#include "../../../include/sbmpi/network/mpi_wrapper.h"
#include "../../../include/sbmpi/util/logging.h"
#include "../../../include/sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Represents the Final Committee, responsible for collecting microblocks
       * from shard leaders and assembling them into macroblocks.
       *
       * Inherits from the base Committee class.
       */
      FinalCommittee::FinalCommittee(MPI_Comm comm, int num_shards)
          : Committee(comm, 0, 0), num_shards(num_shards)
      {
        // Populate the protected members inherited from Committee
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &size);
      }

      /**
       * @brief Destructor for FinalCommittee.
       */
      FinalCommittee::~FinalCommittee() {}

      /**
       * @brief Collects microblocks from all shard leaders.
       *
       * The Final Committee members listen for microblocks sent by the designated
       * shard leaders.
       * @param shardLeaderRanks A vector of global MPI ranks of the shard leaders.
       * @return A vector of collected MicroBlock objects.
       */
      std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
          const std::vector<int>& shardLeaderRanks)
      {
        std::vector<core::blocks::MicroBlock> microBlocks;
        int world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        util::Logger::getLogger().info(
            "Final Committee: Waiting for MicroBlocks from " +
            std::to_string(shardLeaderRanks.size()) + " shards.");

        // Iterate over the specific leader ranks, not generic indices
        for (int leaderRank : shardLeaderRanks) {
          util::Logger::getLogger().debug(
              "Final Committee: Waiting for Shard Leader at Rank " +
              std::to_string(leaderRank));
          // Receive from the specific Shard Leader
          std::vector<char> microBlockData =
              network::recv(leaderRank, 0, MPI_COMM_WORLD);

          core::blocks::MicroBlock microBlock;
          microBlock.deserialize(microBlockData);
          microBlocks.push_back(microBlock);

          util::Logger::getLogger().info(
              "Final Committee: Received MicroBlock from Shard Leader Rank " +
              std::to_string(leaderRank) +
              ". Block Hash: " + microBlock.getHash() +
              ". Tx Count: " + std::to_string(microBlock.transactions.size()));
        }

        return microBlocks;
      }

      /**
       * @brief Assembles a MacroBlock from a collection of MicroBlocks.
       *
       * This process involves adding the microblock hashes to the macroblock
       * and flattening all transactions from the microblocks into the macroblock's
       * transaction list.
       * @param microBlocks A vector of MicroBlock objects collected from shards.
       * @return A newly assembled MacroBlock.
       */
      core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
          const std::vector<core::blocks::MicroBlock>& microBlocks)
      {
        util::Logger::getLogger().info(
            "Final Committee: Assembling MacroBlock from " +
            std::to_string(microBlocks.size()) + " MicroBlocks.");

        core::blocks::MacroBlock macroBlock;

        // In a real system, fetch this from the Blockchain state
        macroBlock.header = core::blocks::BlockHeader(
            0, "prev_hash_placeholder", "merkle_root_placeholder");

        int totalTx = 0;
        for (const auto& microBlock : microBlocks) {
          macroBlock.addMicroBlock(microBlock);

          // Flatten transactions for global state update
          macroBlock.transactions.insert(macroBlock.transactions.end(),
                                         microBlock.transactions.begin(),
                                         microBlock.transactions.end());
          totalTx += microBlock.transactions.size();
        }

        util::Logger::getLogger().info(
            "Final Committee: MacroBlock Assembled. Total Transactions "
            "Finalized: " +
            std::to_string(totalTx));
        return macroBlock;
      }

    } // namespace committee
  } // namespace network
} // namespace sbmpi

```

# src/network/committee/committee.cpp

```cpp
/**
 * @file committee.cpp
 * @brief Implements the base Committee class for managing MPI communicators within committees.
 */
#include "../../../include/sbmpi/network/committee/committee.h"

#include <vector>

#include "../../../include/sbmpi/core/node.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Base class for managing a group of nodes (a committee) using MPI.
       *
       * Provides common functionality for committees, such as accessing their
       * MPI communicator, size, and the rank of the current process within that communicator.
       */
      // Constructor is implicitly defined in header, no need to document here.

      /**
       * @brief Returns the MPI communicator associated with this committee.
       * @return The MPI_Comm object for the committee.
       */
      MPI_Comm Committee::getCommunicator() const
      {
        return communicator;
      }

      /**
       * @brief Returns the total number of processes in this committee.
       * @return The size of the committee.
       */
      int Committee::getSize() const
      {
        return size;
      }

      /**
       * @brief Returns the rank of the current process within this committee's communicator.
       * @return The local rank within the committee.
       */
      int Committee::getRank() const
      {
        return rank;
      }

    } // namespace committee
  } // namespace network
} // namespace sbmpi
```

# include/sbmpi/core/blockchain.h

```cpp
#ifndef SBMPI_BLOCKCHAIN_H
#define SBMPI_BLOCKCHAIN_H

#include <memory>
#include <vector>
#include "blocks/block.h"

namespace sbmpi
{
  namespace core
  {

    class Blockchain
    {
     public:
      Blockchain();
      void                 addBlock(std::unique_ptr<blocks::Block> block);
      const blocks::Block* getBlock(int height) const;
      const blocks::Block* getLatestBlock() const;
      bool                 validate() const;
      int                  getHeight() const;

     private:
      std::vector<std::unique_ptr<blocks::Block>> chain;
      void                                        createGenesisBlock();
    };

  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCKCHAIN_H

```

# include/sbmpi/core/node.h

```cpp
#ifndef SBMPI_NODE_H
#define SBMPI_NODE_H

#include <string>

namespace sbmpi
{
  namespace core
  {

    enum class NodeRole {
      SHARD_MEMBER,
      SHARD_LEADER,
      FINAL_COMMITTEE_MEMBER,
      UNASSIGNED
    };

    class Node
    {
     public:
      Node(int globalRank);
      void     setShardInfo(int id, int rank, NodeRole role);
      int      getGlobalRank() const;
      int      getShardId() const;
      int      getShardRank() const;
      NodeRole getRole() const;

     private:
      int      globalRank;
      int      shardId;
      int      shardRank;
      NodeRole role;
    };

  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_NODE_H

```

# include/sbmpi/core/mempool/mempool.h

```cpp
#ifndef SBMPI_MEMPOOL_H
#define SBMPI_MEMPOOL_H

#include <mutex>
#include <vector>
#include "../state/transaction.h"

namespace sbmpi
{
  namespace core
  {
    namespace mempool
    {

      class Mempool
      {
       public:
        Mempool();
        bool add(const state::Transaction& tx);
        void remove(const std::string& transactionId);
        std::vector<state::Transaction> getTransactions(size_t maxCount);
        size_t                          size() const;

       private:
        mutable std::mutex              mtx;
        std::vector<state::Transaction> transactions;
      };

    }  // namespace mempool
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MEMPOOL_H

```

# include/sbmpi/core/state/transaction.h

```cpp
#ifndef SBMPI_TRANSACTION_H
#define SBMPI_TRANSACTION_H

#include <string>
#include <vector>

/**
 * @file transaction.h
 * @brief Defines the Transaction class, representing a single transaction in
 * the blockchain.
 *
 * The Transaction class encapsulates all data related to a single transaction,
 * including sender and receiver addresses, the amount, and a cryptographic
 * signature. It provides methods for serialization and deserialization to
 * enable network transport via MPI.
 */

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      class Transaction
      {
       public:
        // Unique identifier for the transaction
        std::string id;
        // Address of the sender
        std::string from;
        // Address of the receiver
        std::string to;
        // Amount to be transferred
        double amount;
        // Cryptographic signature of the transaction data
        std::string signature;

        /**
         * @brief Default constructor for Transaction.
         */
        Transaction();

        /**
         * @brief Constructs a Transaction with specified details.
         *
         * @param from The sender's address.
         * @param to The receiver's address.
         * @param amount The amount to be transferred.
         */
        Transaction(const std::string& from, const std::string& to,
                    double amount);

        /**
         * @brief Signs the transaction with a private key.
         *
         * This method should generate a cryptographic signature of the
         * transaction's core data (from, to, amount) to ensure its authenticity
         * and integrity. The actual cryptographic implementation is expected in
         * crypto.cpp.
         *
         * @param privateKey The private key of the sender.
         */
        void sign(const std::string& privateKey);

        /**
         * @brief Verifies the transaction's signature.
         *
         * @return true if the signature is valid, false otherwise.
         */
        bool verify() const;

        /**
         * @brief Serializes the Transaction object into a byte vector for
         * network transmission.
         *
         * @return A std::vector<char> containing the serialized transaction
         * data.
         */
        std::vector<char> serialize() const;

        /**
         * @brief Deserializes a byte vector back into a Transaction object.
         *
         * @param data The byte vector to deserialize.
         */
        void deserialize(const std::vector<char>& data);
      };

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi
#endif  // SBMPI_TRANSACTION_H
```

# include/sbmpi/core/state/genesis.h

```cpp
#ifndef SBMPI_GENESIS_H
#define SBMPI_GENESIS_H

#include <memory>
#include "../blocks/macro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      std::unique_ptr<blocks::MacroBlock> createGenesisBlock();

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_GENESIS_H

```

# include/sbmpi/core/state/state.h

```cpp
#ifndef SBMPI_STATE_H
#define SBMPI_STATE_H

#include <map>
#include <string>
#include "transaction.h"

namespace sbmpi {
namespace core {
namespace state {

class State {
 public:
  State();
  bool apply(const Transaction& tx);
  double getBalance(const std::string& address) const;

 private:
  std::map<std::string, double> balances;
};

}  // namespace state
}  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_STATE_H

```

# include/sbmpi/core/blocks/macro_block.h

```cpp
#ifndef SBMPI_MACRO_BLOCK_H
#define SBMPI_MACRO_BLOCK_H

#include <vector>
#include "block.h"
#include "micro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class MacroBlock : public Block
      {
       public:
        std::vector<std::string> microBlockHashes;

        MacroBlock();
        std::string       getType() const override;
        void              addMicroBlock(const MicroBlock& microBlock);
        std::vector<char> serialize() const override;
        void              deserialize(const std::vector<char>& data) override;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MACRO_BLOCK_H

```

# include/sbmpi/core/blocks/micro_block.h

```cpp
#ifndef SBMPI_MICRO_BLOCK_H
#define SBMPI_MICRO_BLOCK_H

#include "block.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class MicroBlock : public Block
      {
       public:
        int shardId;

        MicroBlock();
        MicroBlock(int shardId);
        std::string       getType() const override;
        std::vector<char> serialize() const override;
        void              deserialize(const std::vector<char>& data) override;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_MICRO_BLOCK_H

```

# include/sbmpi/core/blocks/blockheader.h

```cpp
#ifndef SBMPI_BLOCKHEADER_H
#define SBMPI_BLOCKHEADER_H

#include <chrono>
#include <string>
#include <vector>

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class BlockHeader
      {
       public:
        int                                   height;
        std::string                           previousHash;
        std::string                           merkleRoot;
        std::chrono::system_clock::time_point timestamp;

        BlockHeader();
        BlockHeader(int height, const std::string& previousHash,
                    const std::string& merkleRoot);
        std::string       hash() const;
        std::vector<char> serialize() const;
        void              deserialize(const std::vector<char>& data);
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCKHEADER_H
```

# include/sbmpi/core/blocks/block.h

```cpp
#ifndef SBMPI_BLOCK_H
#define SBMPI_BLOCK_H

#include <string>
#include <vector>
#include "../state/transaction.h"
#include "blockheader.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      class Block
      {
       public:
        BlockHeader                     header;
        std::vector<state::Transaction> transactions;

        virtual ~Block() = default;
        std::string               getHash() const;
        virtual std::string       getType() const               = 0;
        virtual std::vector<char> serialize() const             = 0;
        virtual void deserialize(const std::vector<char>& data) = 0;

       protected:
        Block() = default;
      };

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_BLOCK_H

```

# include/sbmpi/util/timer.h

```cpp
#ifndef SBMPI_TIMER_H
#define SBMPI_TIMER_H

#include <chrono>

namespace sbmpi {
namespace util {

class Timer
{
 public:
  void start();
  void stop();
  double elapsedSeconds() const;
  double elapsedMilliseconds() const;

 private:
  std::chrono::high_resolution_clock::time_point startTime;
  std::chrono::high_resolution_clock::time_point endTime;
};

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_TIMER_H

```

# include/sbmpi/util/logging.h

```cpp
#ifndef SBMPI_LOGGING_H
#define SBMPI_LOGGING_H

#include <string>
#include <cstdio> // Required for snprintf
#include "sbmpi/util/errors.h"

namespace sbmpi
{
  namespace util
  {

    enum class LogLevel {
      NONE  = 0,
      ERROR = 1,
      INFO  = 2,
      DEBUG = 3,
      FATAL = 4
    };

    class Logger
    {
     private:
      // Private constructor for Singleton pattern
      Logger() = default;

      // Static log level (shared across the process)
      static LogLevel loggerLevel;

      // The MPI rank of this process (defaults to -1 until configured)
      int rank = -1;

     public:
      // Delete copy constructors
      Logger(const Logger&)         = delete;
      void operator=(const Logger&) = delete;

      /**
       * @brief Access the Singleton Logger instance.
       */
      static Logger& getLogger()
      {
        static Logger instance;
        return instance;
      }

      /**
       * @brief Configure the logger with the MPI rank.
       * Should be called immediately after MPI_Init.
       */
      void configure(int rank);

      /**
       * @brief Set the verbosity level.
       */
      void setLevel(LogLevel level);

      /**
       * @brief Log a message if the level permits.
       */
      void log(LogLevel level, const std::string& message);

      // Convenience helpers
      void info(const std::string& message);
      void error(const std::string& message);
      void debug(const std::string& message);

      // Non-variadic fatal method (existing)
      void fatal(ErrorCode errorCode, const std::string& message);

      // Variadic template fatal method for printf-style formatting
      template <typename... Args>
      void fatal(ErrorCode errorCode, const char* format, Args... args) {
          // Use a fixed-size buffer to format the message.
          // A more robust solution for production might involve dynamic allocation
          // or C++20's std::format.
          char buffer[1024]; // Max 1023 characters + null terminator
          snprintf(buffer, sizeof(buffer), format, args...);
          this->fatal(errorCode, std::string(buffer)); // Call the non-variadic fatal
      }
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_LOGGING_H
// #ifndef SBMPI_LOGGING_H
// #define SBMPI_LOGGING_H

// #include <iostream>
// #include <sstream>
// #include <string>

// namespace sbmpi
// {
//   namespace util
//   {

//     enum class LogLevel {
//       NONE,
//       ERROR,
//       INFO,
//       DEBUG
//     };

//     class Logger
//     {
//      private:
//       Logger() = default;
//       static LogLevel loggerLevel;

//      public:
//       static Logger& getLogger()
//       {
//         static Logger instance;
//         return instance;
//       }

//       void setLevel(LogLevel level)
//       {
//         loggerLevel = level;
//       }

//       void log(LogLevel level, const std::string& message)
//       {
//         if (level <= loggerLevel) {
//           std::cout << message << std::endl;
//         }
//       }
//     };
//   }  // namespace util
// }  // namespace sbmpi

// #endif  // SBMPI_LOGGING_H

```

# include/sbmpi/util/generator.h

```cpp
#ifndef SBMPI_GENERATOR_H
#define SBMPI_GENERATOR_H

#include <vector>
#include "../core/state/transaction.h"

namespace sbmpi
{
  namespace util
  {

    /**
     * @brief Generates a deterministic set of mock transactions.
     * * This function uses a fixed random seed to ensure that the workload
     * is identical across different simulation runs, allowing for accurate
     * benchmarking of the sharding speedup.
     * * @param count The number of transactions to generate.
     * @return A vector of fully populated and signed Transaction objects.
     */
    std::vector<sbmpi::core::state::Transaction> generateMockTransactions(
        size_t count);

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_GENERATOR_H
```

# include/sbmpi/util/threadpool.h

```cpp
#ifndef SBMPI_THREADPOOL_H
#define SBMPI_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sbmpi
{
  namespace util
  {

    class ThreadPool
    {
     public:
      ThreadPool(size_t threads) : stop(false)
      {
        for (size_t i = 0; i < threads; ++i)
          workers.emplace_back([this] {
            for (;;) {
              std::function<void()> task;
              {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                this->condition.wait(lock, [this] {
                  return this->stop || !this->tasks.empty();
                });
                if (this->stop && this->tasks.empty()) return;
                task = std::move(this->tasks.front());
                this->tasks.pop();
              }
              task();
            }
          });
      }

      template <class F, class... Args>
      auto enqueue(F&& f, Args&&... args)
          -> std::future<typename std::result_of<F(Args...)>::type>
      {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
          tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
      }

      ~ThreadPool()
      {
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) worker.join();
      }

     private:
      std::vector<std::thread>           workers;
      std::queue<std::function<void()> > tasks;

      std::mutex              queue_mutex;
      std::condition_variable condition;
      bool                    stop;
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_THREADPOOL_H
```

# include/sbmpi/util/crypto.h

```cpp
#ifndef SBMPI_CRYPTO_H
#define SBMPI_CRYPTO_H

#include <string>
#include <vector>
#include "../core/state/transaction.h"

namespace sbmpi {
namespace util {

std::string sha256(const std::string& data);
std::string sign(const std::string& data, const std::string& privateKey);
bool verify(const std::string& data, const std::string& signature,
            const std::string& publicKey);
std::string merkle(const std::vector<core::state::Transaction>& transactions);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_CRYPTO_H

```

# include/sbmpi/util/config.h

```cpp
#ifndef SBMPI_CONFIG_H
#define SBMPI_CONFIG_H

#include <string>

namespace sbmpi
{
  namespace util
  {

    class Config
    {
     public:
      int numNodes        = 0;
      int numShards       = 1;
      int numTransactions = 1000;
      int verbose         = 1;

      bool parse(int argc, char** argv);
      void print() const;
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_CONFIG_H

```

# include/sbmpi/util/metrics.h

```cpp
#ifndef SBMPI_METRICS_H
#define SBMPI_METRICS_H

#include <string>

namespace sbmpi
{
  namespace util
  {

    class Metrics
    {
     public:
      static void   recordTime(const std::string& experimentName,
                               double totalTime, int numTransactions);
      static double calculateThroughput(double totalTime, int numTransactions);
      static void   save(const std::string& filepath);
    };

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_METRICS_H

```

# include/sbmpi/util/errors.h

```cpp
#ifndef SBMPI_ERRORS_H
#define SBMPI_ERRORS_H

#include <string>

namespace sbmpi {
namespace util {

enum class ErrorCode {
  SUCCESS = 0,
  MPI_INIT_FAILED,
  INVALID_ARGUMENTS,
};

void fatal(ErrorCode code, const std::string& message);

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_ERRORS_H

```

# include/sbmpi/util/serialization.h

```cpp
#ifndef SBMPI_SERIALIZATION_H
#define SBMPI_SERIALIZATION_H

#include <string>
#include <vector>

namespace sbmpi
{
  namespace util
  {

    void pack(int value, std::vector<char>& buffer);
    void pack(double value, std::vector<char>& buffer);
    void pack(const std::string& value, std::vector<char>& buffer);

    int         unpack_int(const std::vector<char>& buffer, int& offset);
    double      unpack_double(const std::vector<char>& buffer, int& offset);
    std::string unpack_string(const std::vector<char>& buffer, int& offset);

  }  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_SERIALIZATION_H

```

# include/sbmpi/network/mpi_wrapper.h

```cpp
#ifndef SBMPI_MPI_WRAPPER_H
#define SBMPI_MPI_WRAPPER_H

#include <vector>
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm);
    std::vector<char> recv(int source, int tag, MPI_Comm comm);
    void              bcast(std::vector<char>& data, int root, MPI_Comm comm);

  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_MPI_WRAPPER_H

```

# include/sbmpi/network/cross_shard.h

```cpp
#ifndef SBMPI_CROSS_SHARD_H
#define SBMPI_CROSS_SHARD_H

#include "../core/state/transaction.h"

namespace sbmpi
{
  namespace network
  {

    bool isCrossShard(const core::state::Transaction& tx);
    void processTransaction(const core::state::Transaction& tx);

  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_CROSS_SHARD_H

```

# include/sbmpi/network/shard.h

```cpp
#ifndef SBMPI_SHARD_H
#define SBMPI_SHARD_H

#include <vector>
#include "../core/blocks/micro_block.h"
#include "../core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    class Shard
    {
     public:
      Shard(int id, MPI_Comm comm, int leaderRank);
      ~Shard();
      void addTransaction(const core::state::Transaction& tx);
      MPI_Comm getCommunicator() const;
      core::blocks::MicroBlock runConsensus();
      int                      getId() const;

     private:
      int                                   id;
      MPI_Comm                              communicator;
      int                                   leaderRank;
      std::vector<core::state::Transaction> mempool;
    };

  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_SHARD_H

```

# include/sbmpi/network/committee/final_committee.h

```cpp
#ifndef SBMPI_FINAL_COMMITTEE_H
#define SBMPI_FINAL_COMMITTEE_H

#include <vector>
#include "../../core/blocks/macro_block.h"
#include "../../core/blocks/micro_block.h"
#include "committee.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Manages the Final Committee responsible for block aggregation.
       * * This class handles the logic for receiving validated MicroBlocks from
       * Shard Leaders and aggregating them into a final MacroBlock for the
       * global blockchain.
       */
      class FinalCommittee : public Committee
      {
       public:
        /**
         * @brief Constructor.
         * @param comm The MPI communicator for the final committee.
         * @param numShards The total number of shards in the system.
         */
        FinalCommittee(MPI_Comm comm,
                       int      numShards = 0);  // Default arg for flexibility

        ~FinalCommittee();

        /**
         * @brief Collects MicroBlocks from all Shard Leaders.
         * * This method blocks until it has received one MicroBlock from every
         * specified shard leader rank.
         * * @param shardLeaderRanks A vector containing the global ranks of all
         * Shard Leaders.
         * @return A vector of the collected, deserialized MicroBlock objects.
         */
        std::vector<core::blocks::MicroBlock> collectMicroBlocks(
            const std::vector<int>& shardLeaderRanks);

        /**
         * @brief Assembles a MacroBlock from a list of MicroBlocks.
         * * Creates a new MacroBlock containing the hashes of the provided
         * MicroBlocks and aggregates their transactions.
         * * @param microBlocks The vector of MicroBlocks to include.
         * @return The newly created MacroBlock.
         */
        core::blocks::MacroBlock assembleMacroBlock(
            const std::vector<core::blocks::MicroBlock>& microBlocks);

       private:
        int num_shards;
        // Removed 'MPI_Comm communicator' to avoid shadowing the base class
        // member
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_FINAL_COMMITTEE_H
```

# include/sbmpi/network/committee/committee.h

```cpp
#ifndef SBMPI_COMMITTEE_H
#define SBMPI_COMMITTEE_H

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      /**
       * @brief Base class representing a group of nodes (a committee) in the
       * network.
       * * Provides basic functionality common to all committees, such as
       * accessing the MPI communicator and committee metadata.
       */
      class Committee
      {
       public:
        /**
         * @brief Constructor.
         * @param comm The MPI communicator for this committee.
         * @param size The number of nodes in this committee.
         * @param rank The rank of this node within the committee.
         */
        Committee(MPI_Comm comm, int size, int rank)
            : communicator(comm), size(size), rank(rank)
        {
        }

        virtual ~Committee() = default;

        /**
         * @brief Get the MPI Communicator for this committee.
         * @return The MPI_Comm object.
         */
        MPI_Comm getCommunicator() const;

        /**
         * @brief Get the size of the committee (number of nodes).
         * @return The size as an integer.
         */
        int getSize() const;

        /**
         * @brief Get the rank of the current node within this committee.
         * @return The rank as an integer.
         */
        int getRank() const;

       protected:
        MPI_Comm communicator;
        int      size;
        int      rank;
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_COMMITTEE_H
```

# include/sbmpi/consensus/pbft_messages.h

```cpp
#ifndef SBMPI_PBFT_MESSAGES_H
#define SBMPI_PBFT_MESSAGES_H

#include <string>
#include <vector>
#include "pbft.h"

/**
 * @file pbft_messages.h
 * @brief Defines the structures and serialization for messages used in the PBFT
 * protocol.
 *
 * This file provides the definitions for the different message types
 * (PrePrepare, Prepare, Commit) used in the PBFT consensus algorithm. It also
 * declares the functions for serializing and deserializing these messages for
 * network transport. The implementations are in
 * `src/consensus/pbft_messages.cpp`.
 */

// Forward declare the PBFTMessage struct from pbft.h
namespace sbmpi
{
  namespace consensus
  {

    struct PBFTMessage;

    std::vector<char> serializeMessage(const PBFTMessage& msg);
    PBFTMessage       deserializeMessage(const std::vector<char>& data);

  }  // namespace consensus
}  // namespace sbmpi

#endif  // SBMPI_PBFT_MESSAGES_H

```

# include/sbmpi/consensus/pbft.h

```cpp
#ifndef SBMPI_PBFT_H
#define SBMPI_PBFT_H

#include <vector>
#include "../core/blocks/micro_block.h"
#include "../core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace consensus
  {

    enum class PBFTMessageType {
      PRE_PREPARE,
      PREPARE,
      COMMIT
    };

    struct PBFTMessage {
      PBFTMessageType type;
      int             senderId;
      std::string     blockHash;
    };

    class PBFT
    {
     public:
      PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes);
      core::blocks::MicroBlock run(
          const std::vector<core::state::Transaction>& transactions,
          const std::string&                           previousHash);

     private:
      MPI_Comm communicator;
      int      myRank;
      int      leaderRank;
      int      numNodes;
      int      maxFaultyNodes;

      void prePrepare(const core::blocks::MicroBlock& block);
      void prepare(const std::string& blockHash);
      void commit(const std::string& blockHash);
      void broadcastMessage(const PBFTMessage& msg);
    };

  }  // namespace consensus
}  // namespace sbmpi

#endif  // SBMPI_PBFT_H

```


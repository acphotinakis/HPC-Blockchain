# src/main.cpp

```cpp
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
#include "../include/sbmpi/util/generator.h"  // For mock data generation
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
    std::string msg = "Total nodes (" + std::to_string(world_size) +
                      ") must be at least " + std::to_string(MIN_PROCESSES) +
                      " (Shards + FC Size).";

    logger.fatal(ErrorCode::INVALID_ARGUMENTS, msg);
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
  Node myNode(world_rank);
  myNode.setShardInfo(shard_color, shard_rank, role);

  // --- Phase 3: Object Instantiation ---
  if (role == NodeRole::SHARD_LEADER || role == NodeRole::SHARD_MEMBER) {
    // Shard members get a Shard object
    myShard = std::make_unique<sbmpi::network::Shard>(
        myNode.getShardId(), shard_comm, fc_leader_global_rank);

  } else if (role == NodeRole::FINAL_COMMITTEE_MEMBER ||
             role == NodeRole::FINAL_COMMITTEE_MEMBER) {
    // Final committee nodes get a FinalCommittee object
    finalCommittee =
        std::make_unique<sbmpi::network::committee::FinalCommittee>(shard_comm);
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
      int shardId = std::stoi(tx.id) % config.numShards;
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

      // Serialize the vector of transactions before sending
      std::vector<char> buffer;
      auto&             tx_list = partitioned_txs[shardId];
      pack(static_cast<int>(tx_list.size()), buffer);
      for (const auto& tx : tx_list) {
        std::vector<char> tx_data = tx.serialize();
        pack(static_cast<int>(tx_data.size()), buffer);
        buffer.insert(buffer.end(), tx_data.begin(), tx_data.end());
      }

      // Send using MPIWrapper
      sbmpi::network::send(buffer, shardLeaderGlobalRank, 0, MPI_COMM_WORLD);
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

```

# src/consensus/pbft_messages.cpp

```cpp
#include "../../include/sbmpi/consensus/pbft_messages.h"
#include <vector>
#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace consensus
  {

    std::vector<char> serializeMessage(const PBFTMessage& msg)
    {
      std::vector<char> buffer;
      util::pack(static_cast<int>(msg.type), buffer);
      util::pack(msg.senderId, buffer);
      util::pack(msg.blockHash, buffer);
      return buffer;
    }

    PBFTMessage deserializeMessage(const std::vector<char>& data)
    {
      PBFTMessage msg;
      int         offset = 0;
      msg.type = static_cast<PBFTMessageType>(util::unpack_int(data, offset));
      msg.senderId  = util::unpack_int(data, offset);
      msg.blockHash = util::unpack_string(data, offset);
      return msg;
    }

  }  // namespace consensus
}  // namespace sbmpi

```

# src/consensus/pbft.cpp

```cpp
#include "../../include/sbmpi/consensus/pbft.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../include/sbmpi/consensus/pbft_messages.h"
#include "../../include/sbmpi/network/mpi_wrapper.h"
#include "../../include/sbmpi/util/crypto.h"

namespace sbmpi
{
  namespace consensus
  {

    PBFT::PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes)
        : communicator(comm),
          myRank(rank),
          leaderRank(leaderRank),
          numNodes(numNodes)
    {
      // PBFT tolerance: f = (n-1)/3. Quorum = 2f + 1.
      maxFaultyNodes = (numNodes - 1) / 3;
    }

    void PBFT::broadcastMessage(const PBFTMessage& msg)
    {
      std::vector<char> data = serializeMessage(msg);
      for (int i = 0; i < numNodes; ++i) {
        if (i != myRank) {
          network::send(data, i, 0, communicator);
        }
      }
    }

    void PBFT::prePrepare(const core::blocks::MicroBlock& block)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::PRE_PREPARE;
      msg.senderId  = myRank;
      msg.blockHash = block.getHash();

      // 1. Broadcast the full block first (simplification for simulation)
      std::vector<char> blockData = block.serialize();
      network::bcast(blockData, leaderRank, communicator);

      // 2. Broadcast the Pre-Prepare consensus message
      broadcastMessage(msg);
    }

    // FIX: Added blockHash parameter to ensure integrity
    void PBFT::prepare(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::PREPARE;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;  // FIX: Now setting the hash
      broadcastMessage(msg);
    }

    // FIX: Added blockHash parameter
    void PBFT::commit(const std::string& blockHash)
    {
      PBFTMessage msg;
      msg.type      = PBFTMessageType::COMMIT;
      msg.senderId  = myRank;
      msg.blockHash = blockHash;  // FIX: Now setting the hash
      broadcastMessage(msg);
    }

    core::blocks::MicroBlock PBFT::run(
        const std::vector<core::state::Transaction>& transactions)
    {
      core::blocks::MicroBlock block;

      // --- PHASE 0: PRE-PREPARE ---
      if (myRank == leaderRank) {
        std::string merkleRoot = util::merkle(transactions);
        // Note: "genesis_hash_placeholder" should be replaced by referencing
        // the Blockchain state
        block.header = core::blocks::BlockHeader(1, "genesis_hash_placeholder",
                                                 merkleRoot);
        block.transactions = transactions;

        prePrepare(block);
      } else {
        // Replicas receive the block content
        std::vector<char> blockData;
        network::bcast(blockData, leaderRank, communicator);
        block.deserialize(blockData);
      }

      std::string proposedBlockHash = block.getHash();
      int         quorum            = 2 * maxFaultyNodes + 1;

      // --- PHASE 1: PREPARE ---

      // If I am a replica, I need to wait for the PRE-PREPARE message logic
      // (Simplified here assuming the bcast above acted as the data delivery)
      // Now I participate in the Prepare phase.
      if (myRank != leaderRank) {
        prepare(proposedBlockHash);
      }

      // FIX: Non-blocking / Any-Source Receive Loop for PREPARE
      // We wait until we get 2f+1 PREPARE votes (including our own)
      int prepareCount = 0;
      if (myRank == leaderRank)
        prepareCount++;  // Leader implicitly prepares
      else
        prepareCount++;  // I implicitly voted by sending prepare above

      std::set<int> prepareVoters;
      prepareVoters.insert(myRank);

      while (prepareCount < quorum) {
        MPI_Status status;
        // FIX: Use MPI_Probe to wait for a message from ANY source in this
        // communicator
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int               source  = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage       msg     = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::PREPARE &&
            msg.blockHash == proposedBlockHash) {
          if (prepareVoters.find(msg.senderId) == prepareVoters.end()) {
            prepareVoters.insert(msg.senderId);
            prepareCount++;
          }
        }
        // Note: In a full implementation, we would buffer COMMIT messages
        // received early here
      }

      // --- PHASE 2: COMMIT ---

      // We have a Prepared Certificate. Now we Multicast COMMIT.
      commit(proposedBlockHash);

      // FIX: Non-blocking / Any-Source Receive Loop for COMMIT
      int           commitCount = 1;  // My own commit vote
      std::set<int> commitVoters;
      commitVoters.insert(myRank);

      while (commitCount < quorum) {
        MPI_Status status;
        MPI_Probe(MPI_ANY_SOURCE, 0, communicator, &status);

        int               source  = status.MPI_SOURCE;
        std::vector<char> msgData = network::recv(source, 0, communicator);
        PBFTMessage       msg     = deserializeMessage(msgData);

        if (msg.type == PBFTMessageType::COMMIT &&
            msg.blockHash == proposedBlockHash) {
          if (commitVoters.find(msg.senderId) == commitVoters.end()) {
            commitVoters.insert(msg.senderId);
            commitCount++;
          }
        }
      }

      // --- CONSENSUS REACHED ---
      return block;
    }

  }  // namespace consensus
}  // namespace sbmpi
   //

```

# src/core/blockchain.cpp

```cpp
#include "../../include/sbmpi/core/blockchain.h"

#include <memory>
#include <vector>

#include "../../include/sbmpi/core/state/genesis.h"

namespace sbmpi
{
  namespace core
  {

    Blockchain::Blockchain()
    {
      createGenesisBlock();
    }

    void Blockchain::createGenesisBlock()
    {
      chain.push_back(state::createGenesisBlock());
    }

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

    const blocks::Block* Blockchain::getBlock(int height) const
    {
      if (height >= 0 && height < chain.size()) {
        return chain[height].get();
      }
      return nullptr;
    }

    const blocks::Block* Blockchain::getLatestBlock() const
    {
      if (chain.empty()) {
        return nullptr;
      }
      return chain.back().get();
    }

    bool Blockchain::validate() const
    {
      if (chain.size() <= 1) {
        return true;
      }
      for (size_t i = 1; i < chain.size(); ++i) {
        const auto& current  = chain[i];
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

    int Blockchain::getHeight() const
    {
      return chain.empty() ? -1 : static_cast<int>(chain.size()) - 1;
    }

  }  // namespace core
}  // namespace sbmpi
```

# src/core/node.cpp

```cpp
#include "../../include/sbmpi/core/node.h"
#include <string>

namespace sbmpi
{
  namespace core
  {

    Node::Node(int globalRank)
        : globalRank(globalRank),
          shardId(-1),
          shardRank(-1),
          role(NodeRole::SHARD_MEMBER)
    {
    }

    void Node::setShardInfo(int id, int rank, NodeRole role)
    {
      shardId    = id;
      shardRank  = rank;
      this->role = role;
    }

    int Node::getGlobalRank() const
    {
      return globalRank;
    }

    int Node::getShardId() const
    {
      return shardId;
    }

    int Node::getShardRank() const
    {
      return shardRank;
    }

    NodeRole Node::getRole() const
    {
      return role;
    }

  }  // namespace core
}  // namespace sbmpi

```

# src/core/mempool/mempool.cpp

```cpp
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

      Mempool::Mempool() {}

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

      std::vector<state::Transaction> Mempool::getTransactions(size_t maxCount)
      {
        std::lock_guard<std::mutex> lock(mtx);
        size_t count = std::min(maxCount, transactions.size());
        std::vector<state::Transaction> result(transactions.begin(),
                                               transactions.begin() + count);
        transactions.erase(transactions.begin(), transactions.begin() + count);
        return result;
      }

      size_t Mempool::size() const
      {
        std::lock_guard<std::mutex> lock(mtx);
        return transactions.size();
      }

    }  // namespace mempool
  }  // namespace core
}  // namespace sbmpi

```

# src/core/blocks/blockheader.cpp

```cpp
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

      // -------------------------------------------------------------
      // Utility: convert bytes to hex string
      // -------------------------------------------------------------
      static std::string toHex(const unsigned char* data, std::size_t len)
      {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (std::size_t i = 0; i < len; ++i)
          oss << std::setw(2) << static_cast<int>(data[i]);

        return oss.str();
      }

      // -------------------------------------------------------------
      // Default constructor
      // -------------------------------------------------------------
      BlockHeader::BlockHeader()
          : height(0),
            previousHash(""),
            merkleRoot(""),
            timestamp(std::chrono::system_clock::now())
      {
      }

      // -------------------------------------------------------------
      // Main constructor
      // -------------------------------------------------------------
      BlockHeader::BlockHeader(int height_, const std::string& previousHash_,
                               const std::string& merkleRoot_)
          : height(height_),
            previousHash(previousHash_),
            merkleRoot(merkleRoot_),
            timestamp(std::chrono::system_clock::now())
      {
      }

      // -------------------------------------------------------------
      // Cryptographic Hash (SHA-256)
      // -------------------------------------------------------------
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

      // -------------------------------------------------------------
      // Serialization
      // Format:
      //   [height:int32]
      //   [timestamp:int64 ms]
      //   [prevHashLen:int32][prevHash bytes]
      //   [merkleLen:int32][merkle bytes]
      // -------------------------------------------------------------
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

      // -------------------------------------------------------------
      // Deserialization
      // -------------------------------------------------------------
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

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi
```

# src/core/blocks/micro_block.cpp

```cpp
#include "../../../include/sbmpi/core/blocks/micro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      MicroBlock::MicroBlock() : shardId(0) {}

      MicroBlock::MicroBlock(int shardId) : shardId(shardId) {}

      std::string MicroBlock::getType() const
      {
        return "MicroBlock";
      }

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

      void MicroBlock::deserialize(const std::vector<char>& data)
      {
        int offset = 0;

        int               headerSize = util::unpack_int(data, offset);
        std::vector<char> headerVec(data.begin() + offset,
                                    data.begin() + offset + headerSize);
        header.deserialize(headerVec);
        offset += headerSize;

        shardId = util::unpack_int(data, offset);

        transactions.clear();
        int numTransactions = util::unpack_int(data, offset);
        for (int i = 0; i < numTransactions; ++i) {
          int                txSize = util::unpack_int(data, offset);
          std::vector<char>  txData(data.begin() + offset,
                                    data.begin() + offset + txSize);
          state::Transaction tx;
          tx.deserialize(txData);
          transactions.push_back(tx);
          offset += txSize;
        }
      }

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

```

# src/core/blocks/macro_block.cpp

```cpp
#include "../../../include/sbmpi/core/blocks/macro_block.h"

#include <vector>

#include "../../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace core
  {
    namespace blocks
    {

      MacroBlock::MacroBlock() {}

      std::string MacroBlock::getType() const
      {
        return "MacroBlock";
      }

      void MacroBlock::addMicroBlock(const MicroBlock& microBlock)
      {
        microBlockHashes.push_back(microBlock.getHash());
      }

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

      void MacroBlock::deserialize(const std::vector<char>& data)
      {
        int offset = 0;

        // Deserialize header
        int               headerSize = util::unpack_int(data, offset);
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
          int                txSize = util::unpack_int(data, offset);
          std::vector<char>  txData(data.begin() + offset,
                                    data.begin() + offset + txSize);
          state::Transaction tx;
          tx.deserialize(txData);
          transactions.push_back(tx);
          offset += txSize;
        }
      }

    }  // namespace blocks
  }  // namespace core
}  // namespace sbmpi

```

# src/core/blocks/block.cpp

```cpp
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

      std::string Block::getHash() const
      {
        return header.hash();
      }

    }  // namespace blocks
  }  // namespace core

}  // namespace sbmpi
```

# src/core/state/state.cpp

```cpp
#include "../../../include/sbmpi/core/state/state.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      State::State()
      {
        // Maybe some initial state, e.g. for genesis
        balances["genesis_address"] = 1000000.0;
      }

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

      double State::getBalance(const std::string& address) const
      {
        auto it = balances.find(address);
        if (it != balances.end()) {
          return it->second;
        }
        return 0.0;
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

```

# src/core/state/transaction.cpp

```cpp
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

      Transaction::Transaction() : amount(0.0) {}

      Transaction::Transaction(const std::string& from, const std::string& to,
                               double amount)
          : from(from), to(to), amount(amount)
      {
        std::stringstream ss;
        ss << from << to << amount
           << std::chrono::system_clock::now().time_since_epoch().count();
        id = util::sha256(ss.str());
      }

      void Transaction::sign(const std::string& privateKey)
      {
        std::string data = from + to + std::to_string(amount);
        signature        = util::sign(data, privateKey);
      }

      bool Transaction::verify() const
      {
        std::string data = from + to + std::to_string(amount);
        // Assumes the 'from' address is the public key for dummy verification
        return util::verify(data, signature, from);
      }

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

      void Transaction::deserialize(const std::vector<char>& data)
      {
        int offset = 0;
        id         = util::unpack_string(data, offset);
        from       = util::unpack_string(data, offset);
        to         = util::unpack_string(data, offset);
        amount     = util::unpack_double(data, offset);
        signature  = util::unpack_string(data, offset);
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

```

# src/core/state/genesis.cpp

```cpp
#include "../../../include/sbmpi/core/state/genesis.h"
#include <memory>
#include "../../../include/sbmpi/core/blocks/macro_block.h"

namespace sbmpi
{
  namespace core
  {
    namespace state
    {

      std::unique_ptr<sbmpi::core::blocks::MacroBlock> createGenesisBlock()
      {
        auto genesisBlock = std::make_unique<sbmpi::core::blocks::MacroBlock>();
        genesisBlock->header.height       = 0;
        genesisBlock->header.previousHash = "0";
        // In a real scenario, merkle root would be calculated from genesis
        // transactions
        genesisBlock->header.merkleRoot = "0";
        genesisBlock->header.timestamp  = std::chrono::system_clock::now();
        return genesisBlock;
      }

    }  // namespace state
  }  // namespace core
}  // namespace sbmpi

```

# src/util/generator.cpp

```cpp
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

  }  // namespace util
}  // namespace sbmpi
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
#include "../../include/sbmpi/util/timer.h"

namespace sbmpi
{
  namespace util
  {

    void Timer::start()
    {
      startTime = std::chrono::high_resolution_clock::now();
    }

    void Timer::stop()
    {
      endTime = std::chrono::high_resolution_clock::now();
    }

    double Timer::elapsedSeconds() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double>>(
                 endTime - startTime)
          .count();
    }

    double Timer::elapsedMilliseconds() const
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                   startTime)
          .count();
    }

  }  // namespace util
}  // namespace sbmpi

```

# src/util/crypto.cpp

```cpp
#include "../../include/sbmpi/util/crypto.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include <openssl/sha.h>
#include <vector>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <string>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    std::string sha256(const std::string& data)
    {
      unsigned char hash[SHA256_DIGEST_LENGTH];
      SHA256_CTX    sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256, data.c_str(), data.size());
      SHA256_Final(hash, &sha256);
      std::stringstream ss;
      for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
      }
      return ss.str();
    }

    std::string sign(const std::string& data, const std::string& privateKey)
    {
      // Dummy implementation
      return sha256(data + privateKey);
    }

    bool verify(const std::string& data, const std::string& signature,
                const std::string& publicKey)
    {
      // Dummy implementation
      return signature == sha256(data + publicKey);
    }

    /**
    * @brief Calculates the Merkle root hash given a vector of 'Transactions'.
    * @param transactions A reference to a std::vector containing core::state:Transaction instances.
    * @return A std::string representation of the calculate Merkle root. 
    */
    std::string merkle(const std::vector<core::state::Transaction>& transactions)
    {
      // Cannot compute merkle root with empty set of TXs
      if (transactions.empty()) 
      {
        return "";
      }

      // Create a vector containing the IDs (hashes) of the TXs
      std::vector<std::string> currentTransactions;
      for (const auto& tx : transactions) {
        // Debug
        //std::cout << "TX:[" << tx.id << "]" << std::endl;
        currentTransactions.push_back(tx.id);
      }

      // Combine the hashes until the merkle root is reached
      while (currentTransactions.size() != 1)
      {
        // Create a new vector containing the combined hashes
        std::vector<std::string> newTransactions;
        // Iterate the current vector of transactions by steps of 2
        for (size_t i = 0; i < currentTransactions.size(); i+=2)
        {
          // If two hashes can be accessed, then hash the combination of the two neighboring hashes
          if (i+1 < currentTransactions.size())
          {
            std::string newHash = sha256(currentTransactions[i] + currentTransactions[i+1]);
            newTransactions.push_back(newHash);
          }
          // Otherwise, add the single edge hash to the new vector of hashes
          else
          {
            newTransactions.push_back(currentTransactions[i]);
          }
        }
        // Reference the new vector at the end of iterations
        currentTransactions = newTransactions;
      }

      // Return the merkle root
      return currentTransactions[0];
    }

  }  // namespace util
}  // namespace sbmpi

```

# src/util/logging.cpp

```cpp
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

    void Logger::configure(int r)
    {
      this->rank = r;
    }

    void Logger::setLevel(LogLevel level)
    {
      loggerLevel = level;
    }

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

    void Logger::info(const std::string& message)
    {
      log(LogLevel::INFO, message);
    }

    void Logger::error(const std::string& message)
    {
      log(LogLevel::ERROR, message);
    }

    void Logger::debug(const std::string& message)
    {
      log(LogLevel::DEBUG, message);
    }

    void Logger::fatal(ErrorCode code, const std::string& message)
    {
      log(LogLevel::FATAL, message);
      // CRITICAL: Also call the global fatal function to terminate the program.
      // The global fatal function is responsible for printing to stderr and exiting.
      sbmpi::util::fatal(code, message);
    }

  }  // namespace util
}  // namespace sbmpi
// #include "../../include/sbmpi/util/logging.h"
// #include <iostream>

// namespace sbmpi
// {
//   namespace util
//   {

//     LogLevel Logger::loggerLevel = LogLevel::INFO;

//   }  // namespace util
// }  // namespace sbmpi

```

# src/util/metrics.cpp

```cpp
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
      struct ExperimentResult {
        std::string name;
        double      totalTime;
        int         numTransactions;
      };
      std::map<std::string, ExperimentResult> results;
    }  // namespace

    void Metrics::recordTime(const std::string& experimentName,
                             double totalTime, int numTransactions)
    {
      results[experimentName] = {experimentName, totalTime, numTransactions};
    }

    double Metrics::calculateThroughput(double totalTime, int numTransactions)
    {
      if (totalTime == 0) return 0;
      return numTransactions / totalTime;
    }

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
#include "../../include/sbmpi/util/errors.h"
#include <cstdlib>
#include <iostream>

namespace sbmpi
{
  namespace util
  {

    void fatal(ErrorCode code, const std::string& message)
    {
      std::cerr << "Fatal Error [" << static_cast<int>(code) << "]: " << message
                << std::endl;
      exit(static_cast<int>(code));
    }

  }  // namespace util
}  // namespace sbmpi

```

# src/util/serialization.cpp

```cpp
#include "../../include/sbmpi/util/serialization.h"
#include <cstring>

namespace sbmpi
{
  namespace util
  {

    void pack(int value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(int));
    }

    void pack(double value, std::vector<char>& buffer)
    {
      const char* bytes = reinterpret_cast<const char*>(&value);
      buffer.insert(buffer.end(), bytes, bytes + sizeof(double));
    }

    void pack(const std::string& value, std::vector<char>& buffer)
    {
      int len = value.length();
      pack(len, buffer);
      buffer.insert(buffer.end(), value.begin(), value.end());
    }

    int unpack_int(const std::vector<char>& buffer, int& offset)
    {
      int value;
      std::memcpy(&value, buffer.data() + offset, sizeof(int));
      offset += sizeof(int);
      return value;
    }

    double unpack_double(const std::vector<char>& buffer, int& offset)
    {
      double value;
      std::memcpy(&value, buffer.data() + offset, sizeof(double));
      offset += sizeof(double);
      return value;
    }

    std::string unpack_string(const std::vector<char>& buffer, int& offset)
    {
      int         len = unpack_int(buffer, offset);
      std::string value(buffer.data() + offset, len);
      offset += len;
      return value;
    }

  }  // namespace util
}  // namespace sbmpi

```

# src/util/linkedlist.cpp

```cpp

```

# src/util/config.cpp

```cpp
#include "../../include/sbmpi/util/config.h"
#include <iostream>
#include <string>

namespace sbmpi
{
  namespace util
  {

    // Assuming the definition of Config::parse is in config.cpp
    // and the declarations of numNodes, numShards, numTransactions, verbose
    // are in config.h

    bool Config::parse(int argc, char** argv)
    {
      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // --- FIX: Handle Long Flags from Makefile ---
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

    void Config::print() const
    {
      std::cout << "Configuration:" << std::endl;
      std::cout << "  Nodes: " << numNodes << std::endl;
      std::cout << "  Shards: " << numShards << std::endl;
      std::cout << "  Transactions: " << numTransactions << std::endl;
      std::cout << "  Verbose: " << verbose << std::endl;
    }

  }  // namespace util
}  // namespace sbmpi
```

# src/util/threadpool.cpp

```cpp

```

# src/network/cross_shard.cpp

```cpp
#include "../../include/sbmpi/network/cross_shard.h"

#include "../../include/sbmpi/core/state/transaction.h"

namespace sbmpi
{
  namespace network
  {

    bool isCrossShard(const core::state::Transaction& tx)
    {
      // This is a placeholder. In a real system, this would involve checking
      // if the sender and receiver addresses belong to different shards.
      // For now, let's assume all transactions are intra-shard.
      return false;
    }

    void processTransaction(const core::state::Transaction& tx)
    {
      // This is a placeholder. In a real system, cross-shard transactions
      // would be routed to the appropriate shard.
    }

  }  // namespace network
}  // namespace sbmpi

```

# src/network/mpi_wrapper.cpp

```cpp
#include "../../include/sbmpi/network/mpi_wrapper.h"

#include <vector>

#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    void send(const std::vector<char>& data, int dest, int tag, MPI_Comm comm)
    {
      MPI_Send(data.data(), static_cast<int>(data.size()), MPI_CHAR, dest, tag,
               comm);
    }

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

  }  // namespace network
}  // namespace sbmpi

```

# src/network/shard.cpp

```cpp
#include "../../include/sbmpi/network/shard.h"

#include <vector>

#include "../../include/sbmpi/core/blocks/micro_block.h"
#include "../../include/sbmpi/core/state/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {

    Shard::Shard(int id, MPI_Comm comm, int leaderRank)
        : id(id), communicator(comm), leaderRank(leaderRank)
    {
    }

    Shard::~Shard() {}

    void Shard::addTransaction(const core::state::Transaction& tx)
    {
      mempool.push_back(tx);
    }

    MPI_Comm Shard::getCommunicator() const
    {
      return communicator;
    }

    core::blocks::MicroBlock Shard::runConsensus()
    {
      // This is a placeholder. In a real implementation, this would involve
      // running a consensus algorithm (e.g., PBFT) with transactions from the
      // mempool.
      return core::blocks::MicroBlock(id);
    }

    int Shard::getId() const
    {
      return id;
    }

  }  // namespace network
}  // namespace sbmpi
```

# src/network/committee/final_committee.cpp

```cpp
#include "../../../include/sbmpi/network/committee/final_committee.h"

#include <iostream>
#include <vector>

#include "../../../include/sbmpi/core/blocks/macro_block.h"
#include "../../../include/sbmpi/core/blocks/micro_block.h"
#include "../../../include/sbmpi/network/mpi_wrapper.h"
#include "../../../include/sbmpi/util/serialization.h"
#include "mpi.h"

namespace sbmpi
{
  namespace network
  {
    namespace committee
    {

      FinalCommittee::FinalCommittee(MPI_Comm comm, int num_shards)
          : communicator(comm), num_shards(num_shards)
      {
      }

      FinalCommittee::~FinalCommittee() {}

      // FIX: Changed signature to accept specific ranks of shard leaders
      std::vector<core::blocks::MicroBlock> FinalCommittee::collectMicroBlocks(
          const std::vector<int>& shardLeaderRanks)
      {
        std::vector<core::blocks::MicroBlock> microBlocks;
        int                                   world_rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        // FIX: Iterate over the specific leader ranks, not generic indices
        for (int leaderRank : shardLeaderRanks) {
          // Receive from the specific Shard Leader
          std::vector<char> microBlockData =
              network::recv(leaderRank, 0, MPI_COMM_WORLD);

          core::blocks::MicroBlock microBlock;
          microBlock.deserialize(microBlockData);
          microBlocks.push_back(microBlock);
        }

        return microBlocks;
      }

      core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
          const std::vector<core::blocks::MicroBlock>& microBlocks)
      {
        core::blocks::MacroBlock macroBlock;

        // In a real system, fetch this from the Blockchain state
        macroBlock.header = core::blocks::BlockHeader(
            0, "prev_hash_placeholder", "merkle_root_placeholder");

        for (const auto& microBlock : microBlocks) {
          macroBlock.addMicroBlock(microBlock);

          // Flatten transactions for global state update
          macroBlock.transactions.insert(macroBlock.transactions.end(),
                                         microBlock.transactions.begin(),
                                         microBlock.transactions.end());
        }
        return macroBlock;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi
   //
// #include "sbmpi/network/committee/final_committee.h"

// #include <vector>

// #include "mpi.h"
// #include "sbmpi/core/blocks/macro_block.h"
// #include "sbmpi/core/blocks/micro_block.h"
// #include "sbmpi/network/mpi_wrapper.h"
// #include "sbmpi/util/serialization.h"

// namespace sbmpi
// {
//   namespace network
//   {
//     namespace committee
//     {

//       FinalCommittee::FinalCommittee(MPI_Comm comm) : communicator(comm) {}

//       FinalCommittee::~FinalCommittee() {}

//       std::vector<core::blocks::MicroBlock>
//       FinalCommittee::collectMicroBlocks(
//           int numShards)
//       {
//         std::vector<core::blocks::MicroBlock> microBlocks;
//         int                                   world_rank;
//         MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

//         for (int i = 0; i < numShards; ++i) {
//           // Assuming each shard leader sends its microblock to the final
//           // committee This is a simplified model. In a real system, there
//           would
//           // be a more complex routing and verification process.
//           std::vector<char> microBlockData = network::recv(i, 0,
//           communicator); core::blocks::MicroBlock microBlock;
//           microBlock.deserialize(microBlockData);
//           microBlocks.push_back(microBlock);
//         }
//         return microBlocks;
//       }

//       core::blocks::MacroBlock FinalCommittee::assembleMacroBlock(
//           const std::vector<core::blocks::MicroBlock>& microBlocks)
//       {
//         core::blocks::MacroBlock macroBlock;
//         // In a real system, the previous hash would come from the
//         blockchain.
//         // The merkle root would be calculated from the micro-block hashes
//         and
//         // transactions.
//         macroBlock.header = core::blocks::BlockHeader(
//             0, "prev_hash_placeholder", "merkle_root_placeholder");

//         for (const auto& microBlock : microBlocks) {
//           macroBlock.addMicroBlock(microBlock);
//           // Also add transactions from microblocks to macroblock
//           (simplified)
//           macroBlock.transactions.insert(macroBlock.transactions.end(),
//                                          microBlock.transactions.begin(),
//                                          microBlock.transactions.end());
//         }
//         return macroBlock;
//       }

//     }  // namespace committee
//   }  // namespace network
// }  // namespace sbmpi

```

# src/network/committee/committee.cpp

```cpp
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

      MPI_Comm Committee::getCommunicator() const
      {
        return communicator;
      }

      int Committee::getSize() const
      {
        return size;
      }

      int Committee::getRank() const
      {
        return rank;
      }

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi
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

namespace sbmpi {
namespace util {

class ThreadPool {
public:
    ThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i<threads; ++i)
            workers.emplace_back(
                [this] {
                    for(;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty(); });
                            if(this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                }
            );
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
            
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }

private:
    std::vector< std::thread > workers;
    std::queue< std::function<void()> > tasks;
    
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_THREADPOOL_H
```

# include/sbmpi/util/linkedlist.h

```cpp
#ifndef SBMPI_LINKEDLIST_H
#define SBMPI_LINKEDLIST_H

#include <iostream>

namespace sbmpi {
namespace util {

template <typename T>
class LinkedListNode {
 public:
  T data;
  LinkedListNode<T>* prev;
  LinkedListNode<T>* next;

  LinkedListNode(T data) : data(data), prev(nullptr), next(nullptr) {}
};

template <typename T>
class LinkedList {
 public:
  LinkedList() : head(nullptr), tail(nullptr), count(0) {}
  ~LinkedList() {
    LinkedListNode<T>* current = head;
    while (current) {
      LinkedListNode<T>* next = current->next;
      delete current;
      current = next;
    }
  }

  void append(T data) {
    LinkedListNode<T>* newNode = new LinkedListNode<T>(data);
    if (!tail) {
      head = tail = newNode;
    } else {
      tail->next = newNode;
      newNode->prev = tail;
      tail = newNode;
    }
    count++;
  }

  void prepend(T data) {
    LinkedListNode<T>* newNode = new LinkedListNode<T>(data);
    if (!head) {
      head = tail = newNode;
    } else {
      head->prev = newNode;
      newNode->next = head;
      head = newNode;
    }
    count++;
  }

  bool remove(T data) {
    LinkedListNode<T>* current = head;
    while (current) {
      if (current->data == data) {
        if (current->prev) {
          current->prev->next = current->next;
        } else {
          head = current->next;
        }
        if (current->next) {
          current->next->prev = current->prev;
        } else {
          tail = current->prev;
        }
        delete current;
        count--;
        return true;
      }
      current = current->next;
    }
    return false;
  }

  void print() const {
    LinkedListNode<T>* current = head;
    while (current) {
      std::cout << current->data << " ";
      current = current->next;
    }
    std::cout << std::endl;
  }

  int size() const { return count; }

 private:
  LinkedListNode<T>* head;
  LinkedListNode<T>* tail;
  int count;
};

}  // namespace util
}  // namespace sbmpi

#endif  // SBMPI_LINKEDLIST_H
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

namespace sbmpi {
namespace util {

void pack(int value, std::vector<char>& buffer);
void pack(double value, std::vector<char>& buffer);
void pack(const std::string& value, std::vector<char>& buffer);

int unpack_int(const std::vector<char>& buffer, int& offset);
double unpack_double(const std::vector<char>& buffer, int& offset);
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
        int      num_shards;
        MPI_Comm communicator;
      };

    }  // namespace committee
  }  // namespace network
}  // namespace sbmpi

#endif  // SBMPI_FINAL_COMMITTEE_H
// #ifndef SBMPI_FINAL_COMMITTEE_H
// #define SBMPI_FINAL_COMMITTEE_H

// #include <vector>
// #include "../../core/blocks/macro_block.h"
// #include "../../core/blocks/micro_block.h"
// #include "mpi.h"

// namespace sbmpi
// {
//   namespace network
//   {
//     namespace committee
//     {

//       class FinalCommittee
//       {
//        public:
//         FinalCommittee(MPI_Comm comm);
//         ~FinalCommittee();
//         std::vector<core::blocks::MicroBlock> collectMicroBlocks(int
//         numShards); core::blocks::MacroBlock              assembleMacroBlock(
//                          const std::vector<core::blocks::MicroBlock>&
//                          microBlocks);

//        private:
//         MPI_Comm communicator;
//       };

//     }  // namespace committee
//   }  // namespace network
// }  // namespace sbmpi

// #endif  // SBMPI_FINAL_COMMITTEE_H

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
          const std::vector<core::state::Transaction>& transactions);

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


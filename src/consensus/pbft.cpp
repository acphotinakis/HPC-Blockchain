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
// #include "../../include/sbmpi/consensus/pbft.h"

// #include <map>
// #include <string>
// #include <vector>

// #include "../../include/sbmpi/consensus/pbft_messages.h"
// #include "../../include/sbmpi/network/mpi_wrapper.h"
// #include "../../include/sbmpi/util/crypto.h"

// namespace sbmpi
// {
//   namespace consensus
//   {

//     PBFT::PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes)
//         : communicator(comm),
//           myRank(rank),
//           leaderRank(leaderRank),
//           numNodes(numNodes)
//     {
//       maxFaultyNodes = (numNodes - 1) / 3;
//     }

//     void PBFT::broadcastMessage(const PBFTMessage& msg)
//     {
//       std::vector<char> data = serializeMessage(msg);
//       for (int i = 0; i < numNodes; ++i) {
//         if (i != myRank) {
//           network::send(data, i, 0, communicator);
//         }
//       }
//     }

//     void PBFT::prePrepare(const core::blocks::MicroBlock& block)
//     {
//       PBFTMessage msg;
//       msg.type      = PBFTMessageType::PRE_PREPARE;
//       msg.senderId  = myRank;
//       msg.blockHash = block.getHash();

//       // In a real implementation, we would also send the block itself.
//       // For this simulation, we assume nodes can reconstruct the block from
//       // transactions. Or we can broadcast the block first. Let's assume the
//       // leader does that.
//       std::vector<char> blockData = block.serialize();
//       network::bcast(blockData, leaderRank, communicator);

//       broadcastMessage(msg);
//     }

//     void PBFT::prepare()
//     {
//       PBFTMessage msg;
//       msg.type     = PBFTMessageType::PREPARE;
//       msg.senderId = myRank;
//       // blockHash should be set from the received pre-prepare message
//       // This logic is part of the `run` method.
//       broadcastMessage(msg);
//     }

//     void PBFT::commit()
//     {
//       PBFTMessage msg;
//       msg.type     = PBFTMessageType::COMMIT;
//       msg.senderId = myRank;
//       // blockHash should be set from the received pre-prepare message
//       broadcastMessage(msg);
//     }

//     core::blocks::MicroBlock PBFT::run(
//         const std::vector<core::state::Transaction>& transactions)
//     {
//       core::blocks::MicroBlock block;

//       // 1. Leader proposes a block
//       if (myRank == leaderRank) {
//         // In a real system, we'd get previous block hash from blockchain
//         std::string merkleRoot = util::merkle(
//             transactions);  // Calculate merkle root from the transactions
//         block.header = core::blocks::BlockHeader(1,
//         "genesis_hash_placeholder",
//                                                  merkleRoot);
//         block.transactions = transactions;
//         prePrepare(block);
//       } else {
//         // Non-leaders receive the broadcasted block
//         std::vector<char> blockData;
//         // The bcast function requires data to be pre-sized if not root.
//         // For simplicity, let's assume a max block size or use a probe-recv
//         // pattern. For now, let's assume the blockData is correctly sized by
//         // the bcast. This is a simplification for the current context.
//         network::bcast(blockData, leaderRank, communicator);
//         block.deserialize(blockData);
//       }

//       // PBFT phases
//       std::map<std::string, int> prepareVotes;
//       std::map<std::string, int> commitVotes;
//       std::string                proposedBlockHash = block.getHash();

//       // 2. All nodes enter PREPARE phase after receiving PRE-PREPARE
//       if (myRank != leaderRank) {
//         // Receive PRE-PREPARE message
//         // This part needs to be more robust, handling multiple messages and
//         // timeouts. For now, a simple receive from leader.
//         std::vector<char> msgData = network::recv(leaderRank, 0,
//         communicator); PBFTMessage       prePrepareMsg =
//         deserializeMessage(msgData);
//         // Verify prePrepareMsg.blockHash matches the received block.
//         // If valid, send PREPARE message.
//         prepare();
//       }

//       // Collect PREPARE messages
//       // This is a simplified collection. In reality, it would involve
//       // non-blocking receives and timeouts.
//       int prepareCount = 0;
//       if (myRank == leaderRank) {
//         prepareCount = 1;  // Leader implicitly prepares its own block
//       }
//       for (int i = 0; i < numNodes; ++i) {
//         if (i == myRank) continue;
//         // This is blocking, which is not ideal for a real PBFT.
//         // A real implementation would use MPI_Irecv and check for
//         completion. std::vector<char> msgData = network::recv(i, 0,
//         communicator); PBFTMessage       msg     =
//         deserializeMessage(msgData); if (msg.type == PBFTMessageType::PREPARE
//         &&
//             msg.blockHash == proposedBlockHash) {
//           prepareCount++;
//         }
//       }

//       if (prepareCount >= 2 * maxFaultyNodes + 1) {
//         // Enough PREPARE messages received, send COMMIT
//         commit();
//       }

//       // Collect COMMIT messages
//       int commitCount = 0;
//       if (myRank == leaderRank) {
//         commitCount = 1;  // Leader implicitly commits its own block
//       }
//       for (int i = 0; i < numNodes; ++i) {
//         if (i == myRank) continue;
//         std::vector<char> msgData = network::recv(i, 0, communicator);
//         PBFTMessage       msg     = deserializeMessage(msgData);
//         if (msg.type == PBFTMessageType::COMMIT &&
//             msg.blockHash == proposedBlockHash) {
//           commitCount++;
//         }
//       }

//       if (commitCount >= 2 * maxFaultyNodes + 1) {
//         // Enough COMMIT messages received, block is committed
//         return block;
//       } else {
//         // Consensus failed (simplified)
//         // In a real system, this would trigger a view change.
//         return core::blocks::MicroBlock();  // Return an empty block to
//         indicate
//                                             // failure
//       }
//     }

//   }  // namespace consensus
// }  // namespace sbmpi

#ifndef SBMPI_PBFT_H
#define SBMPI_PBFT_H

#include "mpi.h"
#include "../core/transaction.h"
#include "../core/blocks/micro_block.h"
#include <vector>

/**
 * @file pbft.h
 * @brief Defines the interface for the Practical Byzantine Fault Tolerance (PBFT) consensus algorithm.
 *
 * The PBFT class orchestrates the three-phase consensus protocol (pre-prepare,
 * prepare, commit) among a committee of nodes within a given MPI communicator.
 * Its goal is to validate a block of transactions and ensure that all honest
 * nodes agree on the block's contents and order.
 */

// Represents the different types of messages used in the PBFT protocol.
enum class PBFTMessageType {
    PRE_PREPARE,
    PREPARE,
    COMMIT
};

// A struct to represent a PBFT message.
struct PBFTMessage {
    PBFTMessageType type;
    int senderId;
    std::string blockHash;
    // Other relevant data, like view number, sequence number, etc.
};

class PBFT {
public:
    /**
     * @brief Constructor for the PBFT engine.
     *
     * @param comm The MPI communicator for the consensus group.
     * @param rank The rank of the current node in the communicator.
     * @param leaderRank The rank of the leader node.
     * @param numNodes The total number of nodes in the consensus group.
     */
    PBFT(MPI_Comm comm, int rank, int leaderRank, int numNodes);

    /**
     * @brief Executes the full PBFT consensus process on a set of transactions.
     *
     * This method will guide the node through the pre-prepare, prepare, and commit
     * phases. If the node is the leader, it will initiate the process.
     *
     * @param transactions The list of transactions to be included in the block.
     * @return A MicroBlock if consensus is reached, otherwise an empty/invalid block.
     */
    MicroBlock run(const std::vector<Transaction>& transactions);

private:
    MPI_Comm communicator;
    int myRank;
    int leaderRank;
    int numNodes;
    int maxFaultyNodes;

    /**
     * @brief The leader node sends a PRE_PREPARE message with the proposed block.
     */
    void prePrepare(const MicroBlock& block);

    /**
     * @brief Nodes broadcast a PREPARE message after validating the PRE_PREPARE.
     */
    void prepare();

    /**
     * @brief Nodes broadcast a COMMIT message after receiving enough PREPARE messages.
     */
    void commit();

    /**
     * @brief Broadcasts a PBFT message to all nodes in the communicator.
     */
    void broadcastMessage(const PBFTMessage& msg);
};

#endif // SBMPI_PBFT_H
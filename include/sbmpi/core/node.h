#ifndef SBMPI_NODE_H
#define SBMPI_NODE_H

#include <string>

/**
 * @file node.h
 * @brief Defines the Node class, representing a participant in the blockchain network.
 *
 * Each MPI process in the simulation corresponds to a Node. This class holds
 * information about the node's identity, its role within the network (e.g., part
 * of a specific shard or the final committee), and its MPI ranks.
 */

// Defines the possible roles a node can have in the network.
enum class NodeRole {
    // A regular participant in a shard committee.
    SHARD_MEMBER,
    // The leader of a shard committee.
    SHARD_LEADER,
    // A member of the final committee.
    FINAL_COMMITTEE_MEMBER
};

class Node {
public:
    /**
     * @brief Constructor for a Node.
     *
     * @param globalRank The node's rank in the MPI_COMM_WORLD communicator.
     */
    Node(int globalRank);

    /**
     * @brief Sets the shard-specific information for the node.
     *
     * @param id The ID of the shard the node belongs to.
     * @param rank The node's rank within the shard's MPI communicator.
     * @param role The role of the node within the shard (leader or member).
     */
    void setShardInfo(int id, int rank, NodeRole role);

    /**
     * @brief Gets the node's global MPI rank.
     * @return The rank in MPI_COMM_WORLD.
     */
    int getGlobalRank() const;

    /**
     * @brief Gets the ID of the shard this node belongs to.
     * @return The shard ID, or -1 if not part of a shard.
     */
    int getShardId() const;

    /**
     * @brief Gets the node's rank within its shard communicator.
     * @return The shard-local rank.
     */
    int getShardRank() const;

    /**
     * @brief Gets the role of the node.
     * @return The NodeRole enum value.
     */
    NodeRole getRole() const;

private:
    // The node's rank in the global MPI_COMM_WORLD.
    int globalRank;
    // The ID of the shard the node is assigned to.
    int shardId;
    // The node's rank within its shard-specific communicator.
    int shardRank;
    // The role of the node in the network.
    NodeRole role;
};

#endif // SBMPI_NODE_H
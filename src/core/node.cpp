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

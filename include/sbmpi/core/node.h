/**
 * @file node.h
 * @brief Defines the Node class, representing the state of an MPI process.
 *
 * @headerfile node.h
 *
 * @details
 * The corresponding node.cpp will implement methods for state management.
 * This class is a high-level wrapper around an MPI process.
 *
 * This is a central class. After `MPI_Init()`, `main.cpp` will:
 * 1.  Determine the total number of processes (`MPI_Comm_size`).
 * 2.  Partition the processes into Shards and a Final Committee using
 * `MPI_Comm_split`.
 * 3.  Instantiate this `Node` class, passing it the results: its global
 * rank, its new shard-local rank, its role, and its new `MPI_Comm`.
 *
 * Based on its `NodeRole`, the `main.cpp` will then instantiate either a
 * `Shard` or `FinalCommittee` object, passing this Node's info to it.
 */

#pragma once

#include "mpi.h"

namespace sbmpi
{
  namespace core
  {

    /**
     * @enum NodeRole
     * @brief Defines the role of a node in the sharded network.
     */
    enum class NodeRole {
      /**
       * @brief A member of a processing shard (committee).
       */
      SHARD_MEMBER,

      /**
       * @brief The leader (rank 0) of a processing shard.
       */
      SHARD_LEADER,

      /**
       * @brief A member of the final aggregation committee.
       */
      FINAL_COMMITTEE_MEMBER,

      /**
       * @brief The leader (rank 0) of the final committee.
       */
      FINAL_COMMITTEE_LEADER,

      /**
       * @brief A node that is not assigned (e.g., global root).
       */
      UNASSIGNED
    };

    /**
     * @class Node
     * @brief Represents the state and identity of a single MPI process.
     */
    class Node
    {
     public:
      /**
       * @brief Constructor.
       * @param global_rank The node's rank in `MPI_COMM_WORLD`.
       * @param shard_comm The node's local communicator (for its shard or
       * the final committee).
       * @param role The role assigned to this node.
       */
      Node(int global_rank, MPI_Comm shard_comm, NodeRole role);

      /**
       * @brief Destructor.
       * Frees the MPI communicator if it's not MPI_COMM_NULL or
       * MPI_COMM_WORLD.
       */
      ~Node();

      // Deleted copy/move constructors to manage MPI_Comm lifetime
      Node(const Node&)            = delete;
      Node& operator=(const Node&) = delete;
      Node(Node&&)                 = delete;
      Node& operator=(Node&&)      = delete;

      int getGlobalRank() const
      {
        return m_global_rank_;
      }
      int getShardRank() const
      {
        return m_shard_rank_;
      }
      int getShardSize() const
      {
        return m_shard_size_;
      }
      NodeRole getRole() const
      {
        return m_role_;
      }
      MPI_Comm getCommunicator() const
      {
        return m_shard_comm_;
      }

      bool isShardLeader() const
      {
        return m_role_ == NodeRole::SHARD_LEADER;
      }
      bool isFinalCommitteeLeader() const
      {
        return m_role_ == NodeRole::FINAL_COMMITTEE_LEADER;
      }

     private:
      int      m_global_rank_ = -1;  // Rank in MPI_COMM_WORLD
      int      m_shard_rank_  = -1;  // Rank in m_shard_comm_
      int      m_shard_size_  = -1;
      NodeRole m_role_        = NodeRole::UNASSIGNED;
      MPI_Comm m_shard_comm_  = MPI_COMM_NULL;  // The local communicator
    };

  }  // namespace core
}  // namespace sbmpi
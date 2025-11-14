#include "../../include/sbmpi/core/node.h"
#include <mpi.h>
#include <iostream>
#include <stdexcept>

namespace sbmpi
{
  namespace core
  {

    // -------------------- Constructor --------------------
    Node::Node(int global_rank, MPI_Comm shard_comm, NodeRole role)
        : m_global_rank_(global_rank), m_shard_comm_(shard_comm), m_role_(role)
    {
      if (m_shard_comm_ != MPI_COMM_NULL) {
        // Query the shard communicator for rank and size
        MPI_Comm_rank(m_shard_comm_, &m_shard_rank_);
        MPI_Comm_size(m_shard_comm_, &m_shard_size_);
      } else {
        m_shard_rank_ = -1;
        m_shard_size_ = 0;
      }

      // Optional sanity check
      if (role == NodeRole::UNASSIGNED) {
        std::cerr << "Warning: Node created with UNASSIGNED role." << std::endl;
      }
    }

    // -------------------- Destructor --------------------
    Node::~Node()
    {
      // Only free the communicator if it is valid and not MPI_COMM_WORLD
      if (m_shard_comm_ != MPI_COMM_NULL && m_shard_comm_ != MPI_COMM_WORLD) {
        MPI_Comm_free(&m_shard_comm_);
        m_shard_comm_ = MPI_COMM_NULL;
      }
    }

  }  // namespace core
}  // namespace sbmpi

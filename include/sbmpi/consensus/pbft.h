/**
 * @file pbft.h
 * @brief Implements the Practical Byzantine Fault Tolerance (PBFT) consensus.
 *
 * @headerfile pbft.h
 *
 * @details
 * The corresponding pbft.cpp will contain the complex state machine
 * logic for the PBFT algorithm. This is the core "engine" of the
 * project's consensus mechanism.
 *
 * **MPI Interaction:**
 * This module is heavily reliant on MPI. It is instantiated by a `Shard`
 * object and is passed the `MPI_Comm` for that specific shard. All its
 * communication is *intra-shard*.
 *
 * - `prePrepare()`: The SHARD_LEADER (rank 0 in the comm) will use
 * `MPI_Bcast` to send the proposed block to all replicas.
 * - `prepare()`: Replicas will validate the pre-prepare and then use
 * `MPI_Allgather` to broadcast their "prepare" vote to all other
 * nodes in the communicator.
 * - `commit()`: Once a node has 2f prepare messages, it enters the
 * commit phase, again using `MPI_Allgather` to broadcast its "commit"
 * vote.
 *
 * This module's `runConsensus()` function will return a validated `Block`
 * (or a bool indicating success) to the calling `Shard` object.
 */

#pragma once

#include <memory>
#include <vector>
#include "../core/block.h"
#include "../core/transaction.h"
#include "mpi.h"

namespace sbmpi
{
  namespace consensus
  {

    /**
     * @enum PbftState
     * @brief The state of a node within the PBFT protocol.
     */
    enum class PbftState {
      IDLE,
      PRE_PREPARED,
      PREPARED,
      COMMITTED
    };

    /**
     * @class PBFT
     * @brief Manages the PBFT consensus process for a single committee.
     */
    class PBFT
    {
     public:
      /**
       * @brief Constructor.
       * @param committee_comm The MPI communicator for this consensus group.
       * @param rank The rank of this node *within* the committee_comm.
       * @param committee_size The total size of the committee (n).
       * @param faulty_nodes The number of faulty nodes to tolerate (f).
       */
      PBFT(MPI_Comm committee_comm, int rank, int committee_size,
           int faulty_nodes);

      /**
       * @brief Runs one full round of PBFT consensus on a set of transactions.
       *
       * This is the main entry point for the consensus algorithm.
       *
       * @param transactions The pool of transactions to reach consensus on.
       * @return A unique_ptr to the validated Block if consensus is
       * reached, nullptr otherwise.
       */
      std::unique_ptr<core::Block> runConsensus(
          const std::vector<core::Transaction>& transactions);

     private:
      /**
       * @brief The leader node proposes a block and sends a PRE-PREPARE
       * message.
       */
      void prePrepare(const std::vector<core::Transaction>& transactions);

      /**
       * @brief Replicas receive the PRE-PREPARE, validate it, and
       * broadcast a PREPARE message.
       */
      void prepare();

      /**
       * @brief Nodes receive 2f PREPARE messages, validate them, and
       * broadcast a COMMIT message.
       */
      void commit();

      MPI_Comm m_comm_;
      int      m_rank_;
      int      m_size_;    // n (total nodes)
      int      m_faulty_;  // f (faulty nodes)
      int      m_quorum_;  // 2f + 1

      PbftState                    m_state_;
      std::unique_ptr<core::Block> m_proposed_block_;
    };

  }  // namespace consensus
}  // namespace sbmpi
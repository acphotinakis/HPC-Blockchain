/**
 * @file shard.h
 * @brief Manages the lifecycle and operation of a single shard (committee).
 *
 * @headerfile shard.h
 *
 * @details
 * The corresponding shard.cpp will implement the logic for running the
 * consensus cycle. This class acts as the "controller" for all nodes
 * assigned to a processing shard.
 *
 * It is instantiated in `main.cpp` by any process assigned the
 * `NodeRole::SHARD_MEMBER` or `NodeRole::SHARD_LEADER`.
 *
 * **Interaction Flow:**
 * 1.  Receives its `MPI_Comm` from the `Node` object.
 * 2.  Instantiates a `PBFT` module, passing it this communicator.
 * 3.  The `SHARD_LEADER` receives its subset of `Transaction`s (e.g.,
 * via `MPI_Scatter` from the global root).
 * 4.  The `SHARD_LEADER` triggers `runConsensusCycle()`.
 * 5.  `runConsensusCycle()` calls `PBFT::runConsensus()`.
 * 6.  If consensus succeeds, the `SHARD_LEADER` calls
 * `sendBlockToFinalCommittee()`, which performs an `MPI_Send` to the
 * `FINAL_COMMITTEE_LEADER`.
 */

#pragma once

#include "../core/node.h"
#include "../core/block.h"
#include "../core/transaction.h"
#include "../consensus/pbft.h"
#include <memory>
#include <vector>
#include "mpi.h"

namespace sbmpi {
namespace network {

/**
 * @class Shard
 * @brief Manages a single shard, its transaction pool, and its
 * consensus instance.
 */
class Shard {
public:
    /**
     * @brief Constructor.
     * @param node The Node object containing this process's state
     * (communicator, rank, role).
     * @param config The global experiment configuration.
     * @param final_committee_leader_rank The global rank of the final
     * committee's leader, to know
     * who to send the block to.
     */
    Shard(const core::Node& node,
          const util::ExperimentConfig& config,
          int final_committee_leader_rank);

    /**
     * @brief Runs the main loop for a shard node.
     *
     * Waits to receive transactions, runs consensus, and (if leader)
     * sends the resulting block.
     */
    void runMainLoop();

private:
    /**
     * @brief Receives transactions from the global root node.
     */
    void receiveTransactions();

    /**
     * @brief Runs one cycle of PBFT consensus.
     */
    void runConsensusCycle();

    /**
     * @brief (Leader-only) Sends the validated block to the final
     * committee.
     * @param block The validated block to send.
     */
    void sendBlockToFinalCommittee(const core::Block& block);

    const core::Node& m_node_;
    const util::ExperimentConfig& m_config_;
    std::unique_ptr<consensus::PBFT> m_pbft_instance_;
    std::vector<core::Transaction> m_transaction_pool_;

    int m_final_committee_leader_rank_;
};

} // namespace network
} // namespace sbmpi
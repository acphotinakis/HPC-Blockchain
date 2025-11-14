/**
 * @file final_committee.h
 * @brief Manages the final aggregation committee.
 *
 * @headerfile final_committee.h
 *
 * @details
 * The corresponding final_committee.cpp will implement the aggregation
 * logic. This class is instantiated in `main.cpp` by any process assigned
 * the `NodeRole::FINAL_COMMITTEE_MEMBER` or `NodeRole::FINAL_COMMITTEE_LEADER`.
 *
 * **MPI Interaction:**
 * This module's role is to aggregate the results from all parallel shards
 *.
 *
 * The `FINAL_COMMITTEE_LEADER` will execute `listenForBlocks()`, which
 * involves posting non-blocking `MPI_Recv` calls (or a loop of blocking
 * `MPI_Recv`) to receive validated micro-blocks from all `SHARD_LEADER`s
 *.
 *
 * Once all blocks are received, the `FINAL_COMMITTEE_LEADER` assembles
 * them into the `Blockchain` object (`m_global_chain_`) and can signal
 * the end of the simulation.
 */

#pragma once

#include "../core/node.h"
#include "../core/blockchain.h"
#include "../util/config.h"
#include "mpi.h"

namespace sbmpi {
namespace network {

/**
 * @class FinalCommittee
 * @brief Manages the aggregation of blocks from all shards.
 */
class FinalCommittee {
public:
    /**
     * @brief Constructor.
     * @param node The Node object containing this process's state.
     * @param config The global experiment configuration.
     */
    FinalCommittee(const core::Node& node, const util::ExperimentConfig& config);

    /**
     * @brief Runs the main loop for a final committee node.
     *
     * The leader will listen for blocks from shards. Replicas may
     * participate in a final validation step (if designed) or
     * simply wait.
     */
    void runMainLoop();

private:
    /**
     * @brief (Leader-only) Listens for validated blocks from all
     * shard leaders.
     */
    void listenForBlocks();

    /**
     * @brief (Leader-only) Assembles the received blocks into the final
     * chain.
     */
    void assembleFinalChain();

    const core::Node& m_node_;
    const util::ExperimentConfig& m_config_;
    core::Blockchain m_global_chain_; // The final, canonical chain
};

} // namespace network
} // namespace sbmpi
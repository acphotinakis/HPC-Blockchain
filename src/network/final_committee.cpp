#include "../../include/sbmpi/network/final_committee.h"
#include "../../include/sbmpi/util/logging.h"
#include <mpi.h>
#include <vector>

namespace sbmpi
{
  namespace network
  {

    FinalCommittee::FinalCommittee(const core::Node& node, const util::ExperimentConfig& config)
        : m_node_(node), m_config_(config)
    {
    }

    void FinalCommittee::runMainLoop()
    {
      if (m_node_.isFinalCommitteeLeader()) {
        listenForBlocks();
      }
      // Other members of the final committee could participate in validation,
      // but for this simulation, they remain idle.
    }

    void FinalCommittee::listenForBlocks()
    {
      util::Logger logger(m_node_.getGlobalRank());
      logger.info("Final committee leader listening for blocks from shards.");

      std::vector<core::Block> received_blocks;
      MPI_Status status;

      // The number of blocks to expect is the number of shards
      for (int i = 0; i < m_config_.num_shards; ++i) {
        // Buffer size for a SHA-256 hex string + null terminator
        char hash_buffer[65]; 
        MPI_Recv(hash_buffer, 65, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        
        int shard_leader_rank = status.MPI_SOURCE;
        logger.info("Received block hash from shard leader at global rank " + std::to_string(shard_leader_rank));

        // In a real system, we would receive a full block and add it.
        // Here, we just simulate adding a placeholder block to the chain.
        // The shard_id can be inferred if we know the mapping of ranks to shards.
        core::Block received_block(m_global_chain_.getHeight(), m_global_chain_.getLastBlock().getHash(), i);
        m_global_chain_.addBlock(received_block);
      }

      logger.info("All shard blocks received. Final chain height: " + std::to_string(m_global_chain_.getHeight()));
      assembleFinalChain();
    }

    void FinalCommittee::assembleFinalChain()
    {
      // In this simple model, blocks are added to the chain as they are received.
      // A more complex model might sort them or perform further validation.
      util::Logger logger(m_node_.getGlobalRank());
      logger.info("Final chain assembled.");
      // The m_global_chain_ is already assembled in listenForBlocks.
    }

  }  // namespace network
}  // namespace sbmpi
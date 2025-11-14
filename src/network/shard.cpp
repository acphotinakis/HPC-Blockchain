#include "../../include/sbmpi/network/shard.h"
#include "../../include/sbmpi/util/logging.h"
#include <mpi.h>

namespace sbmpi
{
  namespace network
  {

    Shard::Shard(const core::Node& node, const util::ExperimentConfig& config, int final_committee_leader_rank)
        : m_node_(node),
          m_config_(config),
          m_final_committee_leader_rank_(final_committee_leader_rank)
    {
      m_pbft_instance_ = std::make_unique<consensus::PBFT>(
          m_node_.getCommunicator(),
          m_node_.getShardRank(),
          m_node_.getShardSize(),
          m_config_.faulty_nodes_per_shard);
    }

    void Shard::runMainLoop()
    {
      receiveTransactions();
      runConsensusCycle();
    }

    void Shard::receiveTransactions()
    {
      util::Logger logger(m_node_.getGlobalRank());
      int tx_count = 0;

      // Leader receives the transaction count from the root.
      if (m_node_.isShardLeader()) {
        MPI_Status status;
        MPI_Recv(&tx_count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        logger.info("Received " + std::to_string(tx_count) + " transactions.");
        m_transaction_pool_.resize(tx_count);
        
        // Receive the actual transactions
        std::vector<char> buffer(tx_count * sizeof(core::Transaction));
        MPI_Recv(buffer.data(), buffer.size(), MPI_CHAR, 0, 1, MPI_COMM_WORLD, &status);

        size_t offset = 0;
        for(int i=0; i<tx_count; ++i) {
            core::Transaction tx;
            std::vector<char> tx_data(buffer.begin() + offset, buffer.begin() + offset + sizeof(core::Transaction));
            tx.deserialize(tx_data);
            m_transaction_pool_[i] = tx;
            offset += sizeof(core::Transaction);
        }
      }

      // Broadcast transaction count to other shard members
      MPI_Bcast(&tx_count, 1, MPI_INT, 0, m_node_.getCommunicator());

      if (!m_node_.isShardLeader()) {
        m_transaction_pool_.resize(tx_count);
      }
      
      // Broadcast the transactions themselves
      // A more efficient implementation might serialize all transactions into one buffer
      MPI_Bcast(m_transaction_pool_.data(), tx_count * sizeof(core::Transaction), MPI_CHAR, 0, m_node_.getCommunicator());
    }

    void Shard::runConsensusCycle()
    {
      util::Logger logger(m_node_.getGlobalRank());
      logger.info("Starting consensus cycle.");

      auto block = m_pbft_instance_->runConsensus(m_transaction_pool_);

      if (block) {
        logger.info("Consensus successful. Block created.");
        if (m_node_.isShardLeader()) {
          sendBlockToFinalCommittee(*block);
        }
      } else {
        logger.error("Consensus failed.");
      }
    }

    void Shard::sendBlockToFinalCommittee(const core::Block& block)
    {
      util::Logger logger(m_node_.getGlobalRank());
      logger.info("Sending block to final committee leader.");

      // Simple serialization for the block: just send the hash.
      // A real implementation would send the full block or a header with proofs.
      std::string hash = block.calculateHash();
      MPI_Send(hash.c_str(), hash.size() + 1, MPI_CHAR, m_final_committee_leader_rank_, 0, MPI_COMM_WORLD);
    }

  }  // namespace network
}  // namespace sbmpi
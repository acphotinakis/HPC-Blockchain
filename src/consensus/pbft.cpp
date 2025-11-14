#include "../../include/sbmpi/consensus/pbft.h"
#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>

// For simplicity, we'll serialize the block hash for votes.
// A real implementation would use more complex message structures.
constexpr int HASH_SIZE = 64; // SHA-256 hex string

namespace sbmpi
{
  namespace consensus
  {

    PBFT::PBFT(MPI_Comm committee_comm, int rank, int committee_size, int faulty_nodes)
        : m_comm_(committee_comm),
          m_rank_(rank),
          m_size_(committee_size),
          m_faulty_(faulty_nodes),
          m_state_(PbftState::IDLE)
    {
      m_quorum_ = 2 * m_faulty_ + 1;
    }

    std::unique_ptr<core::Block> PBFT::runConsensus(const std::vector<core::Transaction>& transactions)
    {
      m_state_ = PbftState::IDLE;

      // 1. Pre-prepare phase
      prePrepare(transactions);
      if (m_state_ != PbftState::PRE_PREPARED) {
        // Error occurred or not the leader
        return nullptr;
      }

      // 2. Prepare phase
      prepare();
      if (m_state_ != PbftState::PREPARED) {
        std::cerr << "[Rank " << m_rank_ << "] PBFT failed at PREPARE phase." << std::endl;
        return nullptr;
      }

      // 3. Commit phase
      commit();
      if (m_state_ != PbftState::COMMITTED) {
        std::cerr << "[Rank " << m_rank_ << "] PBFT failed at COMMIT phase." << std::endl;
        return nullptr;
      }

      // Consensus successful
      return std::move(m_proposed_block_);
    }

    void PBFT::prePrepare(const std::vector<core::Transaction>& transactions)
    {
      std::vector<char> block_data;
      int block_data_size = 0;

      if (m_rank_ == 0) { // Leader node
        // Create a proposed block
        // For simplicity, previous hash and block number are placeholders
        m_proposed_block_ = std::make_unique<core::Block>(1, "prev_hash_placeholder", 0);
        for(const auto& tx : transactions) {
            m_proposed_block_->addTransaction(tx);
        }
        std::string block_hash = m_proposed_block_->calculateHash();

        // Serialize the block for broadcast
        // Simple serialization: just send the hash and transactions
        std::string serialized_str = block_hash;
        for(const auto& tx : transactions) {
            std::vector<char> tx_data = tx.serialize();
            serialized_str.append(tx_data.begin(), tx_data.end());
        }
        block_data.assign(serialized_str.begin(), serialized_str.end());
        block_data_size = block_data.size();
      }

      // Broadcast the size of the data first
      MPI_Bcast(&block_data_size, 1, MPI_INT, 0, m_comm_);

      // Resize buffer on non-leader nodes
      if (m_rank_ != 0) {
        block_data.resize(block_data_size);
      }

      // Broadcast the actual block data
      MPI_Bcast(block_data.data(), block_data_size, MPI_CHAR, 0, m_comm_);

      // All nodes (including leader) now have the block data
      if (m_rank_ != 0) {
        std::string serialized_str(block_data.begin(), block_data.end());
        std::string block_hash = serialized_str.substr(0, HASH_SIZE);
        
        m_proposed_block_ = std::make_unique<core::Block>(1, "prev_hash_placeholder", 0);
        
        size_t offset = HASH_SIZE;
        while(offset < serialized_str.size()) {
            core::Transaction tx;
            std::vector<char> tx_data(serialized_str.begin() + offset, serialized_str.begin() + offset + sizeof(core::Transaction));
            tx.deserialize(tx_data);
            m_proposed_block_->addTransaction(tx);
            offset += sizeof(core::Transaction);
        }

        // Validate hash
        if (m_proposed_block_->calculateHash() != block_hash) {
            std::cerr << "[Rank " << m_rank_ << "] PRE-PREPARE failed: Invalid hash." << std::endl;
            return; // Invalid proposal
        }
      }
      
      m_state_ = PbftState::PRE_PREPARED;
    }

    void PBFT::prepare()
    {
      if (m_state_ != PbftState::PRE_PREPARED) return;

      std::string block_hash = m_proposed_block_->calculateHash();
      std::vector<char> send_buf(block_hash.begin(), block_hash.end());
      send_buf.resize(HASH_SIZE);

      std::vector<char> recv_buf(m_size_ * HASH_SIZE);

      MPI_Allgather(send_buf.data(), HASH_SIZE, MPI_CHAR,
                    recv_buf.data(), HASH_SIZE, MPI_CHAR, m_comm_);

      std::map<std::string, int> vote_counts;
      for (int i = 0; i < m_size_; ++i) {
        std::string received_hash(recv_buf.begin() + i * HASH_SIZE, recv_buf.begin() + (i + 1) * HASH_SIZE);
        vote_counts[received_hash]++;
      }

      // Check if we have a quorum of PREPARE messages for our proposed block
      if (vote_counts[block_hash] >= m_quorum_) {
        m_state_ = PbftState::PREPARED;
      }
    }

    void PBFT::commit()
    {
      if (m_state_ != PbftState::PREPARED) return;

      std::string block_hash = m_proposed_block_->calculateHash();
      std::vector<char> send_buf(block_hash.begin(), block_hash.end());
      send_buf.resize(HASH_SIZE);

      std::vector<char> recv_buf(m_size_ * HASH_SIZE);

      MPI_Allgather(send_buf.data(), HASH_SIZE, MPI_CHAR,
                    recv_buf.data(), HASH_SIZE, MPI_CHAR, m_comm_);

      std::map<std::string, int> vote_counts;
      for (int i = 0; i < m_size_; ++i) {
        std::string received_hash(recv_buf.begin() + i * HASH_SIZE, recv_buf.begin() + (i + 1) * HASH_SIZE);
        vote_counts[received_hash]++;
      }

      // Check if we have a quorum of COMMIT messages
      if (vote_counts[block_hash] >= m_quorum_) {
        m_state_ = PbftState::COMMITTED;
      }
    }

  }  // namespace consensus
}  // namespace sbmpi
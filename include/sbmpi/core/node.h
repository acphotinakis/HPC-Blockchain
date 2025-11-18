#ifndef SBMPI_NODE_H
#define SBMPI_NODE_H

#include <string>

namespace sbmpi
{
  namespace core
  {

    enum class NodeRole {
      SHARD_MEMBER,
      SHARD_LEADER,
      FINAL_COMMITTEE_MEMBER
    };

    class Node
    {
     public:
      Node(int globalRank);
      void     setShardInfo(int id, int rank, NodeRole role);
      int      getGlobalRank() const;
      int      getShardId() const;
      int      getShardRank() const;
      NodeRole getRole() const;

     private:
      int      globalRank;
      int      shardId;
      int      shardRank;
      NodeRole role;
    };

  }  // namespace core
}  // namespace sbmpi

#endif  // SBMPI_NODE_H

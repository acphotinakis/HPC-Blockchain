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
      FINAL_COMMITTEE_MEMBER,
      UNASSIGNED
    };

    inline std::string nodeRoleToString(NodeRole role) {
      switch (role) {
          case NodeRole::UNASSIGNED: return "UNASSIGNED";
          case NodeRole::SHARD_LEADER: return "SHARD_LEADER";
          case NodeRole::SHARD_MEMBER: return "SHARD_MEMBER";
          case NodeRole::FINAL_COMMITTEE_MEMBER: return "FINAL_COMMITTEE_MEMBER";
          default: return "UNKNOWN";
      }
    }

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

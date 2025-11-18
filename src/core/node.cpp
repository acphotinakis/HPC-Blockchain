#include "../../include/sbmpi/core/node.h"
#include <string>

namespace sbmpi
{
  namespace core
  {

    Node::Node(int globalRank)
        : globalRank(globalRank),
          shardId(-1),
          shardRank(-1),
          role(NodeRole::SHARD_MEMBER)
    {
    }

    void Node::setShardInfo(int id, int rank, NodeRole role)
    {
      shardId    = id;
      shardRank  = rank;
      this->role = role;
    }

    int Node::getGlobalRank() const
    {
      return globalRank;
    }

    int Node::getShardId() const
    {
      return shardId;
    }

    int Node::getShardRank() const
    {
      return shardRank;
    }

    NodeRole Node::getRole() const
    {
      return role;
    }

  }  // namespace core
}  // namespace sbmpi

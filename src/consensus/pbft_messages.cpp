#include "../../include/sbmpi/consensus/pbft_messages.h"

#include <vector>

#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi {
namespace consensus {

std::vector<char> serializeMessage(const PBFTMessage& msg) {
  std::vector<char> buffer;
  util::pack(static_cast<int>(msg.type), buffer);
  util::pack(msg.senderId, buffer);
  util::pack(msg.blockHash, buffer);
  return buffer;
}

PBFTMessage deserializeMessage(const std::vector<char>& data) {
  PBFTMessage msg;
  int offset = 0;
  msg.type = static_cast<PBFTMessageType>(util::unpack_int(data, offset));
  msg.senderId = util::unpack_int(data, offset);
  msg.blockHash = util::unpack_string(data, offset);
  return msg;
}

}  // namespace consensus
}  // namespace sbmpi

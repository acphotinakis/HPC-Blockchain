/**
 * @file pbft_messages.cpp
 * @brief Implements serialization and deserialization for PBFT messages.
 */
#include "../../include/sbmpi/consensus/pbft_messages.h"
#include <vector>
#include "../../include/sbmpi/util/serialization.h"

namespace sbmpi
{
  namespace consensus
  {

    /**
     * @brief Serializes a PBFTMessage into a vector of characters for network transmission.
     * @param msg The PBFTMessage to serialize.
     * @return A std::vector<char> containing the serialized message data.
     */
    std::vector<char> serializeMessage(const PBFTMessage& msg)
    {
      std::vector<char> buffer;
      util::pack(static_cast<int>(msg.type), buffer);
      util::pack(msg.senderId, buffer);
      util::pack(msg.blockHash, buffer);
      return buffer;
    }

    /**
     * @brief Deserializes a vector of characters back into a PBFTMessage object.
     * @param data The std::vector<char> containing the serialized message data.
     * @return A PBFTMessage object reconstructed from the provided data.
     */
    PBFTMessage deserializeMessage(const std::vector<char>& data)
    {
      PBFTMessage msg;
      int offset = 0;
      msg.type = static_cast<PBFTMessageType>(util::unpack_int(data, offset));
      msg.senderId = util::unpack_int(data, offset);
      msg.blockHash = util::unpack_string(data, offset);
      return msg;
    }

  } // namespace consensus
} // namespace sbmpi

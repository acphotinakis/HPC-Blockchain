#ifndef SBMPI_PBFT_MESSAGES_H
#define SBMPI_PBFT_MESSAGES_H

#include <string>
#include <vector>
#include "../consensus/pbft.h"

/**
 * @file pbft_messages.h
 * @brief Defines the structures and serialization for messages used in the PBFT protocol.
 *
 * This file provides the definitions for the different message types (PrePrepare,
 * Prepare, Commit) used in the PBFT consensus algorithm. It also declares the
 * functions for serializing and deserializing these messages for network transport.
 * The implementations are in `src/consensus/pbft_messages.cpp`.
 */

// Forward declare the PBFTMessage struct from pbft.h
struct PBFTMessage;

namespace sbmpi {
namespace consensus {

/**
 * @brief Serializes a generic PBFT message into a byte vector.
 *
 * @param msg The PBFT message to serialize.
 * @return A std::vector<char> containing the serialized message.
 */
std::vector<char> serializeMessage(const PBFTMessage& msg);

/**
 * @brief Deserializes a byte vector into a generic PBFT message.
 *
 * @param data The byte vector to deserialize.
 * @return The deserialized PBFTMessage object.
 */
PBFTMessage deserializeMessage(const std::vector<char>& data);

} // namespace consensus
} // namespace sbmpi

#endif // SBMPI_PBFT_MESSAGES_H
#ifndef SBMPI_BLOCKHEADER_H
#define SBMPI_BLOCKHEADER_H

#include <string>
#include <chrono>

/**
 * @file blockheader.h
 * @brief Defines the BlockHeader class, containing metadata for a block.
 *
 * The BlockHeader is a critical component of a block, storing metadata such as
 * the hash of the previous block, the Merkle root of transactions, a timestamp,
 * and the block's height. This information is used to link blocks together and
 * ensure the integrity of the blockchain.
 */

class BlockHeader {
public:
    // The height of the block in the chain
    int height;
    // The hash of the previous block's header
    std::string previousHash;
    // The Merkle root of all transactions included in this block
    std::string merkleRoot;
    // The timestamp when the block was created
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Default constructor for BlockHeader.
     */
    BlockHeader();

    /**
     * @brief Constructs a BlockHeader with specified details.
     *
     * @param height The block height.
     * @param previousHash The hash of the preceding block.
     * @param merkleRoot The Merkle root of the block's transactions.
     */
    BlockHeader(int height, const std::string& previousHash, const std::string& merkleRoot);

    /**
     * @brief Computes the cryptographic hash of the block header.
     *
     * This hash uniquely identifies the block and is used for linking subsequent blocks.
     *
     * @return A string representing the SHA-256 hash of the header.
     */
    std::string hash() const;

    /**
     * @brief Serializes the BlockHeader into a byte vector.
     *
     * @return A std::vector<char> containing the serialized header data.
     */
    std::vector<char> serialize() const;

    /**
     * @brief Deserializes a byte vector into a BlockHeader object.
     *
     * @param data The byte vector to deserialize.
     */
    void deserialize(const std::vector<char>& data);
};

#endif // SBMPI_BLOCKHEADER_H
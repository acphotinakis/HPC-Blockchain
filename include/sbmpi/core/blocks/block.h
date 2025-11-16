#ifndef SBMPI_BLOCK_H
#define SBMPI_BLOCK_H

#include "blockheader.h"
#include "transaction.h"
#include <vector>
#include <string>

/**
 * @file block.h
 * @brief Defines the abstract base class for all block types in the blockchain.
 *
 * The Block class serves as a polymorphic base for different types of blocks,
 * such as MicroBlock (from shards) and MacroBlock (from the final committee).
 * It defines the common interface and data members, including the block header
 * and a list of transactions.
 */

class Block {
public:
    // The header of the block containing metadata.
    BlockHeader header;
    // The list of transactions included in this block.
    std::vector<Transaction> transactions;

    /**
     * @brief Virtual destructor for the base class.
     */
    virtual ~Block() = default;

    /**
     * @brief Returns the hash of the block.
     *
     * This is typically the hash of the block header.
     *
     * @return The block's hash as a string.
     */
    std::string getHash() const;

    /**
     * @brief Pure virtual function to get the type of the block.
     *
     * @return A string identifying the block type (e.g., "MicroBlock", "MacroBlock").
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Pure virtual function to serialize the block.
     *
     * @return A std::vector<char> containing the serialized block data.
     */
    virtual std::vector<char> serialize() const = 0;

    /**
     * @brief Pure virtual function to deserialize a block.
     *
     * @param data The byte vector to deserialize.
     */
    virtual void deserialize(const std::vector<char>& data) = 0;

protected:
    /**
     * @brief Protected constructor for base class.
     */
    Block() = default;
};

#endif // SBMPI_BLOCK_H
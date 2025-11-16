#ifndef SBMPI_TRANSACTION_H
#define SBMPI_TRANSACTION_H

#include <string>
#include <vector>

/**
 * @file transaction.h
 * @brief Defines the Transaction class, representing a single transaction in
 * the blockchain.
 *
 * The Transaction class encapsulates all data related to a single transaction,
 * including sender and receiver addresses, the amount, and a cryptographic
 * signature. It provides methods for serialization and deserialization to
 * enable network transport via MPI.
 */

class Transaction
{
 public:
  // Unique identifier for the transaction
  std::string id;
  // Address of the sender
  std::string from;
  // Address of the receiver
  std::string to;
  // Amount to be transferred
  double amount;
  // Cryptographic signature of the transaction data
  std::string signature;

  /**
   * @brief Default constructor for Transaction.
   */
  Transaction();

  /**
   * @brief Constructs a Transaction with specified details.
   *
   * @param from The sender's address.
   * @param to The receiver's address.
   * @param amount The amount to be transferred.
   */
  Transaction(const std::string& from, const std::string& to, double amount);

  /**
   * @brief Signs the transaction with a private key.
   *
   * This method should generate a cryptographic signature of the transaction's
   * core data (from, to, amount) to ensure its authenticity and integrity.
   * The actual cryptographic implementation is expected in crypto.cpp.
   *
   * @param privateKey The private key of the sender.
   */
  void sign(const std::string& privateKey);

  /**
   * @brief Verifies the transaction's signature.
   *
   * @return true if the signature is valid, false otherwise.
   */
  bool verify() const;

  /**
   * @brief Serializes the Transaction object into a byte vector for network
   * transmission.
   *
   * @return A std::vector<char> containing the serialized transaction data.
   */
  std::vector<char> serialize() const;

  /**
   * @brief Deserializes a byte vector back into a Transaction object.
   *
   * @param data The byte vector to deserialize.
   */
  void deserialize(const std::vector<char>& data);
};

#endif  // SBMPI_TRANSACTION_H
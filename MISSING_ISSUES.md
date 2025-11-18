# Analysis of Missing and Incorrect Implementations

This document provides a detailed analysis of the logical issues, critical bugs, and incomplete implementations found within the `sbmpi` project codebase. The issues are categorized by severity and component.

---

## 1. Critical Implementation Flaws

These are bugs that fundamentally break the logic of the simulation or the consensus protocol, likely leading to deadlocks, incorrect results, or undefined behavior.

### 1.1. PBFT Implementation is Non-Functional

The PBFT consensus algorithm in `src/consensus/pbft.cpp` is incorrect and will not achieve consensus as intended.

-   **Issue 1: Broken Message Content**
    -   The `prepare()` and `commit()` methods send a `PBFTMessage` without setting the `blockHash` field. The receiver nodes then check this empty hash against the proposed block's hash, a check that will always fail. This completely breaks the voting mechanism.
    -   **File**: `src/consensus/pbft.cpp`
    -   **Fix**: The `prepare()` and `commit()` methods must accept the `blockHash` as an argument and include it in the messages they broadcast.

-   **Issue 2: Sequential and Blocking Message Reception**
    -   The logic for collecting `PREPARE` and `COMMIT` votes uses a blocking `network::recv()` inside a `for` loop that iterates from `i = 0` to `numNodes`. This forces nodes to wait for messages in a fixed, sequential order (e.g., must hear from node 1 before node 2).
    -   **Problem**: In any real distributed system (which MPI simulates), messages arrive out of order. This implementation will deadlock if a higher-ranked node sends its message before a lower-ranked node.
    -   **File**: `src/consensus/pbft.cpp`
    -   **Fix**: The message collection loop should be redesigned to use non-blocking receives. The standard MPI approach is to post multiple `MPI_Irecv` calls (one for each expected peer) and then use `MPI_Waitany` or `MPI_Testsome` in a loop to process messages as they complete, regardless of their arrival order.

### 1.2. Final Committee Reception Logic is Incorrect

-   **Issue**: The `FinalCommittee` in `src/network/committee/final_committee.cpp` attempts to receive `MicroBlock`s from MPI ranks `0, 1, 2, ...`. However, the shard leaders that send these blocks have global ranks determined by their shard assignment (e.g., `0`, `nodesPerShard`, `2 * nodesPerShard`, ...).
-   **Problem**: The `FinalCommittee` will try to receive from nodes that never send a `MicroBlock`, while never receiving the blocks that were actually sent. This will cause the application to hang and deadlock.
-   **File**: `src/network/committee/final_committee.cpp`
-   **Fix**: The `FinalCommittee` needs to know the global ranks of the shard leaders. This list of ranks should be determined in `main.cpp` and passed to `collectMicroBlocks`. The method should then loop over the correct leader ranks when calling `network::recv()`.

### 1.3. Unhandled Nodes in Shard Allocation

-   **Issue**: The node-to-shard allocation logic in `main.cpp` uses integer division: `int nodesPerShard = (world_size - 1) / numShards;`.
-   **Problem**: If `(world_size - 1)` is not perfectly divisible by `numShards`, some MPI processes will not be assigned to a shard or the final committee. These idle processes will do nothing and the simulation will not use all available resources. For example, with 10 nodes and 4 shards, `(10-1)/4 = 2`. Nodes 0-7 become shard members, node 9 is the finalizer, and node 8 is left idle.
-   **File**: `src/main.cpp`
-   **Fix**: The allocation logic should be made more robust. A better approach would be to distribute the remainder nodes across the shards, e.g., `nodes_per_shard = total / n; remainder = total % n;`. The first `remainder` shards get `nodes_per_shard + 1` nodes, and the rest get `nodes_per_shard`.

---

## 2. Logical and Architectural Issues

These are flaws in the high-level design and data flow of the simulation.

### 2.1. Disconnected Blockchain State

-   **Issue**: Each MPI process that acts as a `FinalCommittee` leader initializes its own `Blockchain` object (`sbmpi::core::Blockchain blockchain;`).
-   **Problem**: The "global" blockchain state is not shared or synchronized. In the current code, only one node (the final committee leader) ever adds a block to its local chain. The simulation ends without a consistent, agreed-upon final chain across all nodes. Furthermore, the `previousHash` used for new blocks is always a placeholder, so the blocks are not cryptographically linked.
-   **File**: `src/main.cpp`, `src/core/blockchain.cpp`
-   **Fix**: For a correct simulation, the final `MacroBlock` should be broadcast from the `FinalCommittee` leader to all other nodes so they can update their local copy of the blockchain. The `Blockchain::addBlock` logic should be enhanced to use the hash of the actual latest block as the `previousHash` for the next block.

### 2.2. Centralized and Unrealistic Transaction Handling

-   **Issue**: Transactions are not distributed from a central source. Instead, each shard member generates its own set of transactions and immediately proposes them for consensus within its shard.
-   **Problem**: This does not accurately simulate a real blockchain where transactions come from many sources and are collected in a distributed mempool before being selected for a block. The `Shard::addTransaction` method just adds to a local vector that is immediately consumed, which is not a mempool.
-   **File**: `src/main.cpp`
-   **Fix**: A more realistic simulation would have a designated process (or multiple) generate all transactions and scatter them to the shard leaders. This would better model the "workload distribution" aspect of a sharded system.

---

## 3. Missing or Incomplete Features

These are parts of the code that are placeholders or use dummy implementations, significantly reducing the validity and correctness of the simulation.

### 3.1. Cryptography is a Dummy Implementation

-   **Issue**: The functions in `src/util/crypto.cpp` are not real cryptographic operations.
    -   `sign` is just `sha256(data + privateKey)`.
    -   `verify` is just `signature == sha256(data + publicKey)`.
-   **Problem**: This is fundamentally insecure and does not represent public-key cryptography at all. It provides none of the security guarantees (like non-repudiation) that are essential to a blockchain. The comment in `transaction.cpp` stating `// Assumes the 'from' address is the public key` further highlights this is a placeholder.
-   **File**: `src/util/crypto.cpp`, `src/core/state/transaction.cpp`
-   **Fix**: Replace the dummy functions with a proper cryptographic library like OpenSSL's `EVP` interface for digital signatures (e.g., ECDSA).

### 3.2. Block Integrity is Not Ensured

-   **Issue**: Critical block metadata is hardcoded with placeholder values.
    -   The Merkle Root (`merkle_placeholder`) is never calculated from the block's transactions.
    -   The previous block hash (`genesis_hash_placeholder` or `prev_hash_placeholder`) is not retrieved from the actual previous block.
-   **Problem**: Without a real Merkle root, there is no way to verify the integrity of the transaction set within a block. Without linking to the real previous hash, the "chain" is just a disconnected set of blocks.
-   **File**: `src/consensus/pbft.cpp`, `src/network/committee/final_committee.cpp`
-   **Fix**: Implement a function to calculate the Merkle root from a list of transaction hashes. When creating a new block, fetch the latest block from the blockchain to get its hash and use that as the `previousHash`.

### 3.3. Unused and Incomplete Classes

-   **Issue**: The `Shard::runConsensus()` method is a placeholder and is never called. The core consensus logic is implemented directly in `main.cpp`.
-   **Problem**: This indicates a mismatch between the intended object-oriented design and the actual implementation. The `Shard` class should be responsible for orchestrating its own consensus process.
-   **File**: `include/sbmpi/network/shard.h`, `src/network/shard.cpp`
-   **Fix**: The PBFT logic currently in `main.cpp` should be moved into the `Shard::runConsensus()` method. The `main` function would then simply call `myShard->runConsensus()`.

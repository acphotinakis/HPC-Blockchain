# Analysis of Project Topics

This document provides a detailed explanation of the core concepts behind the *parallelizing-blockchain-computations* project, grounding the discussion in the provided academic literature and the project's C++ codebase.

## Topic 1: Problem Definition & Sharding as a Solution

### Inherent Limitations of Single-Chain Consensus

Traditional blockchain architectures are built on a paradigm of total-order broadcast and global state replication. In such systems, every full node in the network must process every transaction and participate in a global consensus mechanism to validate each new block `~\cite{Yu_ShardinginBlockchains}`. This requirement ensures a single, consistent, and verifiable ledger. However, it creates a significant performance bottleneck, as the throughput of the entire network is limited by the processing capacity of a single node `~\cite{Xu_SurveyofBlockchainConsensusProtocols}`. Consensus protocols like Proof-of-Work (PoW) or standard, non-parallelized Byzantine Fault Tolerance (BFT) implementations all face this limitation, as they require global validation for every state change. This sequential processing model results in low transaction throughput and high latency, hindering the scalability required for large-scale applications.

### Sharding as a Parallel Processing Solution

Sharding is a technique proposed to overcome these limitations by introducing parallelism into the blockchain `~\cite{Yu_ShardinginBlockchains}`. The core idea is to divide the validation workload into disjoint sets of nodes, known as **shards** or **committees**. Instead of a single global state, the network state is partitioned, and each shard is responsible for processing transactions and maintaining only its portion of the state `~\cite{Amiri_ShardingPermissionedBlockchains}`. This allows multiple shards to process transactions concurrently, leading to a theoretical near-linear increase in network throughput with the number of shards `~\cite{Wu2023efficient}`.

In this model:
- The network's nodes are partitioned into multiple shards.
- Transactions are distributed among these shards based on a partitioning rule.
- Each shard runs its own consensus protocol internally to validate a block of transactions in parallel with other shards.
- A mechanism is required to aggregate the results from each shard to produce a final, consistent global state.

---

## Topic 2: Project Overview & Implementation

This project implements a sharded blockchain simulation to demonstrate the scalability outlined above.

### Architecture

The project's architecture, primarily defined in `src/main.cpp`, realizes a hierarchical topology designed for parallel consensus.

1.  **Node Roles & Assignment:** The simulation starts by assigning roles to each MPI process. The `determineNodeAssignment` function in `src/main.cpp` partitions the `world_size` processes into two main groups:
    *   A **Final Committee:** A small, fixed-size group (defined by `FINAL_COMMITTEE_SIZE`) responsible for aggregation.
    *   **Shards:** The remaining processes are divided into a user-specified number of shards (`--shards` parameter). Within each shard, the node with local rank 0 is designated the **Shard Leader** (`NodeRole::SHARD_LEADER`), while others are **Shard Members** (`NodeRole::SHARD_MEMBER`).

2.  **Hierarchical Communication:**
    *   The **Root process** (global rank 0) acts as the initial workload distributor. It generates mock transactions using functions from `src/util/generator.cpp` and sends partitioned batches to the leader of each shard.
    *   **Shard Leaders** receive these transactions and initiate the intra-shard consensus process by proposing a `MicroBlock`. This is handled within the `Shard::runConsensus` method in `src/network/shard.cpp`.
    *   Upon reaching consensus, each Shard Leader sends its validated `MicroBlock` to the **Final Committee** leader.
    *   The **Final Committee** leader, implemented in `src/network/committee/final_committee.cpp`, collects these `MicroBlock`s and assembles them into a final `MacroBlock`, which represents the globally finalized state for that round.

### Technologies

The choice of technologies directly supports the project's goal of simulating a high-performance, distributed system.

-   **C++:** The entire simulation is written in C++ to achieve the high performance necessary for processing a large number of transactions and to allow for low-level control over data structures and memory management, as seen across the `.cpp` and `.h` files in `src/` and `include/`.

-   **MPI (Message Passing Interface):** MPI is used to simulate the distributed, message-passing nature of a blockchain network. The `Makefile` specifies `mpic++` as the compiler, and the codebase uses MPI functions extensively for communication between nodes. A custom wrapper in `src/network/mpi_wrapper.cpp` simplifies sending and receiving serialized data structures. This choice aligns with academic approaches to simulating performance in fault-aware distributed systems `~\cite{HURSEY201215}`.

-   **OpenMP:** To further enhance performance, OpenMP is used for shared-memory parallelism *within* a single node (MPI process). As seen in `src/network/shard.cpp`, when a Shard Leader or Member receives a block proposal, it uses a `#pragma omp parallel for` loop to verify the digital signatures of all transactions in the block concurrently. This accelerates the validation step before consensus voting begins. The use of OpenMP is enabled by the `-fopenmp` flag in the `Makefile`.

### Scope and Assumptions

The simulation operates under a specific set of assumptions that define its scope:

-   **Permissioned Network:** The model assumes a permissioned environment where the identities of all participating nodes are known beforehand. This is reflected in the fixed `world_size` of the MPI simulation and is a fundamental requirement for using a classic BFT consensus protocol like PBFT, which is designed for environments without anonymous participation `~\cite{Zhang_ReachingConsensusintheByzantine}`.

-   **Weak Synchrony (The "Practical" in PBFT):** The project uses **Practical Byzantine Fault Tolerance (PBFT)** as its intra-shard consensus algorithm, as implemented in `src/consensus/pbft.cpp`. PBFT is designed to operate in weakly synchronous systems, meaning it assumes that messages between non-faulty nodes will eventually be delivered, but it does not assume a fixed upper bound on message delay `~\cite{Castro_PracticalByzantineFaultTolerance}`. The message-passing model of MPI is a suitable environment for simulating this assumption. PBFT provides safety and liveness provided that no more than `f = (n-1)/3` nodes in a committee of size `n` are Byzantine (malicious or faulty) `~\cite{Castro_PracticalByzantineFaultTolerance}`. The quorum size of `2f+1` is implemented in the `PBFT::run` method to ensure agreement.

---
## Topic 3: Core Algorithms and Methods

The project's architecture is realized through a set of specific algorithms that manage network structure, consensus, and data integrity.

### a. Algorithms/Methods

#### Network Partitioning (Sharding Logic)

The partitioning of the network into distinct shards and a final committee is a foundational step managed in `src/main.cpp`.

-   **Logic in `determineNodeAssignment`**: This function implements a static partitioning algorithm. It first reserves a set number of processes (`FINAL_COMMITTEE_SIZE`) for the final committee. The remaining pool of processes is then divided among the number of shards specified by the `--shards` command-line argument. The algorithm distributes nodes as evenly as possible, with any remainder nodes being allocated one-by-one to the initial shards. Each process is assigned a `shard_color` (an integer ID for its group) and a `NodeRole` enumeration (e.g., `SHARD_LEADER`, `FINAL_COMMITTEE_MEMBER`).

-   **MPI Communicator Splitting**: The `shard_color` assigned in the previous step is used directly in a call to `MPI_Comm_split(MPI_COMM_WORLD, shard_color, ...)`. This MPI function creates new, isolated communicators from the global set of processes. All processes with the same `shard_color` become part of the same new communicator (`shard_comm`), enabling them to perform intra-shard consensus without interfering with other shards. This is a direct and efficient implementation of the node clustering strategy essential for sharded architectures `~\cite{Amiri_ShardingPermissionedBlockchains}`.

#### Intra-Shard Consensus (PBFT)

Within each shard, consensus is achieved using the Practical Byzantine Fault Tolerance algorithm, as implemented in `src/consensus/pbft.cpp`. This choice is suitable for permissioned environments and provides high performance while tolerating malicious nodes `~\cite{Castro_PracticalByzantineFaultTolerance}`. The implementation follows the classic three-phase protocol:

1.  **Pre-Prepare**: The Shard Leader (rank 0 within the shard's communicator) initiates consensus by broadcasting a `PRE_PREPARE` message containing the hash of a proposed `MicroBlock`. The full block data is also broadcast to all replicas in this phase.
2.  **Prepare**: Replica nodes receive the `MicroBlock` and first validate its contents. This critical validation step is optimized with **multi-threading**: the `Shard::runConsensus` method in `src/network/shard.cpp` uses an OpenMP `#pragma omp parallel for` loop to have a single node verify the cryptographic signatures of all transactions in the block in parallel. This significantly reduces the latency of validating a large batch of transactions. After validation, replica nodes broadcast a `PREPARE` message.
3.  **Commit**: When a node receives a quorum of `2f + 1` matching `PREPARE` messages, it proves that enough honest nodes agree on the proposal, and it enters the "prepared" state. It then broadcasts a `COMMIT` message. Upon receiving a quorum of `COMMIT` messages, the node considers the `MicroBlock` finalized and is ready to move to the next round.

#### Global Aggregation

Once each shard has reached consensus on a `MicroBlock`, the results must be aggregated into the global chain. This logic resides in `src/network/committee/final_committee.cpp`.

-   **`collectMicroBlocks`**: The leader of the Final Committee enters a loop where it waits to receive a finalized `MicroBlock` from each of the known Shard Leader ranks.
-   **`assembleMacroBlock`**: After all `MicroBlock`s are collected, this function is called. It creates a new `MacroBlock` and performs two key actions:
    1.  It records the hash of each `MicroBlock` in the `macroBlock.microBlockHashes` vector.
    2.  It flattens the transactions from all `MicroBlock`s into the `macroBlock.transactions` list.
    This `MacroBlock` is then added to the main `Blockchain` object (managed in `src/core/blockchain.cpp`), creating a canonical, ordered history of all parallel computations.

#### Cryptographic Primitives

The simulation's integrity relies on cryptographic functions implemented in `src/util/crypto.cpp`.

-   **Hashing (Keccak-256)**: The project uses `EVP_sha3_256()` from the OpenSSL library to implement Keccak-256 hashing. This is used to generate deterministic IDs for transactions and to compute the hash of block headers, which is essential for linking blocks together.
-   **Digital Signatures (ECDSA)**: To ensure transaction authenticity, the project integrates the `secp256k1` library. The `sign` function in `crypto.cpp` uses `secp256k1_ecdsa_sign_recoverable` to generate a recoverable signature for each transaction. The corresponding `verify` function uses `secp256k1_ecdsa_recover` to retrieve the signer's public key from the signature and the transaction hash, confirming that only the legitimate owner of the private key could have created it.

### b. Block Diagrams/State Diagrams

Visual diagrams help clarify the project's architecture and control flow.

*   **Figure 1: System Topology**

    *   **Visual Representation**: This diagram illustrates a hierarchical data flow:
        1.  A single **Root** process at the top.
        2.  The Root distributes work to multiple **Shard Leaders**.
        3.  The Shard Leaders report their results to a single **Final Committee**.

    *   **Codebase Context**: This visual flow is directly implemented in the project's MPI communication logic, primarily orchestrated in `src/main.cpp`:
        *   **Root → Shard Leaders**: In `main.cpp`, the process with `world_rank == 0` generates transactions, partitions them, and iterates through each shard to send a workload to the corresponding `shardLeaderGlobalRank`. This is an explicit point-to-point `sbmpi::network::send` call.
        *   **Shard Leaders → Final Committee**: After a shard successfully completes consensus, the `Shard::runConsensus` method (in `src/network/shard.cpp`) has the Shard Leader (the node with `my_shard_rank == 0`) send its finalized `MicroBlock` to the Final Committee leader (`leaderRank`, which is global rank 0). This is received in `FinalCommittee::collectMicroBlocks` (`src/network/committee/final_committee.cpp`).

*   **Figure 2: PBFT State Machine**

    *   **Visual Representation**: This diagram shows the state transitions for a single node during the PBFT consensus protocol: `NEW → PRE-PREPARED → PREPARED → COMMITTED`.

    *   **Codebase Context**: This state machine maps directly to the sequential control flow within the `PBFT::run` method in `src/consensus/pbft.cpp`, which implements the three-phase commit protocol `~\cite{Castro_PracticalByzantineFaultTolerance}`. Although the states are not explicitly defined with an enum, the program logic follows this exact sequence:
        1.  **NEW**: This is the initial state of a node before the `PBFT::run` method begins its main loops.
        2.  **PRE-PREPARED**: A replica node transitions to this state after it receives and validates the leader's `MicroBlock` proposal. It then broadcasts a `PREPARE` message. The code for this is part of the initial block reception and validation at the beginning of `PBFT::run`.
        3.  **PREPARED**: A node enters this state after the first `while (prepareCount < quorum)` loop completes. This loop ensures the node has received `2f + 1` `PREPARE` messages from its peers, confirming that a sufficient number of honest nodes have validated the same proposal. Upon exiting this loop, the node broadcasts a `COMMIT` message.
        4.  **COMMITTED**: The node enters the final state after the second `while (commitCount < quorum)` loop completes, guaranteeing it has received `2f + 1` `COMMIT` messages. At this point, the block is irrevocably committed, and the `PBFT::run` function returns the finalized `MicroBlock`.

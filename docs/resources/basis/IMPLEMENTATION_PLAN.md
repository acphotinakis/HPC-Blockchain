# Implementation Plan: Parallelizing Blockchain Computations (As-Built)

This document outlines the implementation of the C++/MPI project for parallelizing blockchain computations. It has been updated from the original plan to serve as an "as-built" reference, reflecting the final architecture and components.

---

## 1. Project Overview and Goal

The primary objective was to build a C++/MPI simulation demonstrating performance improvements in blockchain transaction processing by comparing a sequential model against a parallel, sharded model. This was successfully achieved by implementing a hierarchical architecture featuring multiple parallel shard committees and a finalization committee.

---

## 2. Phase 1: Foundational Component Implementation

This phase focused on creating the core data structures and classes.

### **`transaction.h` / `transaction.cpp`**
-   **Outcome:** A `Transaction` class was created with an ID, sender/receiver, and amount. Critically, `serialize` and `deserialize` methods were implemented to pack the transaction into a byte buffer for MPI communication.

### **`node.h` / `node.cpp`**
-   **Outcome:** A `Node` class was implemented to store a process's state, including its global MPI rank, its assigned `shardId`, its rank within the shard's communicator, and its `NodeRole` (e.g., `SHARD_LEADER`).

### **Block Hierarchy (`core/blocks/`)**
-   **Outcome:** A sophisticated block hierarchy was implemented.
    -   `Block`: An abstract base class defining a common interface.
    -   `BlockHeader`: A class for block metadata (height, previous hash, Merkle root).
    -   `MicroBlock`: A concrete class representing a block produced by a shard, containing a set of transactions.
    -   `MacroBlock`: A concrete class representing a final block, which aggregates hashes of `MicroBlock`s from all shards.

---

## 3. Phase 2: Baseline & Parallel Model Implementation

This phase implemented the consensus and networking logic for both the sequential (single-shard) and parallel (multi-shard) models.

### **`pbft.h` / `pbft.cpp` & `pbft_messages.h`/`.cpp`**
-   **Outcome:** A `PBFT` class was implemented to manage the `pre-prepare`, `prepare`, and `commit` phases. It operates within a specific `MPI_Comm`, allowing it to run independently within each shard. A separate `PBFTMessage` struct and associated serialization were created to handle the network communication for the protocol's phases.

### **`blockchain.h` / `blockchain.cpp`**
-   **Outcome:** A `Blockchain` class was created to manage the final ledger. It uses a `std::vector<std::unique_ptr<Block>>` to polymorphically store the chain of `MacroBlock`s and includes methods for adding blocks and validating the chain's integrity.

### **`shard.h` / `shard.cpp`**
-   **Outcome:** A `Shard` class was implemented to act as a manager for a shard committee. It holds the shard-specific `MPI_Comm`, a transaction `Mempool`, and a `PBFT` instance. Its main function, `runConsensus()`, orchestrates the process of turning transactions into a validated `MicroBlock`.

### **`final_committee.h` / `final_committee.cpp`**
-   **Outcome:** A `FinalCommittee` class was created to manage the aggregation process. Its `collectMicroBlocks()` method receives `MicroBlock`s from shard leaders, and `assembleMacroBlock()` creates the final `MacroBlock` for the global chain.

---

## 4. Phase 3: Simulation Orchestration & Utilities

This phase focused on the main application logic and the utilities needed to support it.

### **`main.cpp`**
-   **Outcome:** The main orchestrator successfully implements the full simulation workflow:
    1.  Initializes MPI.
    2.  Parses command-line arguments using the `Config` class.
    3.  Partitions MPI processes into `N` shards and one final committee using `MPI_Comm_split`.
    4.  The root process generates and distributes transactions to the shard leaders.
    5.  Each shard runs its `PBFT` consensus process in parallel.
    6.  Shard leaders send their resulting `MicroBlock`s to the `FinalCommittee`.
    7.  The `FinalCommittee` assembles the `MacroBlock` and adds it to the `Blockchain`.
    8.  A `Timer` utility is used to measure the total execution time for performance analysis.

### **Utility Suite (`util/`)**
-   **Outcome:** A robust suite of utilities was developed to support the simulation:
    -   `Config`: For parsing command-line arguments.
    -   `Crypto`: For hashing and cryptographic signatures.
    -   `Logger`: For structured, level-based logging.
    -   `Serializer`: For packing/unpacking data structures for MPI.
    -   `Timer`: For high-resolution performance measurement.

---

## 5. Phase 4: Testing and Verification

-   **`tests/test_pbft.cpp`**: A test suite was created to verify the correctness of the PBFT implementation in isolation, ensuring it could reach consensus under ideal and simulated faulty conditions.
-   **`tests/test_sharding.cpp`**: A test suite was developed to confirm that MPI processes were correctly partitioned into shard and final committee communicators and that transaction distribution worked as expected.

---

## 6. Phase 5: Experimentation and Performance Analysis

-   **`scripts/run_experiment.sh`**: An automation script was written to execute the simulation on the `kraken` cluster with various configurations (e.g., changing the number of shards) and log the results to the `/results` directory.
-   **`scripts/plot_results.py`**: A Python script was created using `pandas` and `matplotlib` to parse the CSV output from the experiments and generate graphs for Throughput, Latency, and Speedup, which were then included in `docs/PERFORMANCE.md`.
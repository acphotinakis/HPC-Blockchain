# Project Concept Map: Parallelizing Blockchain Computations

This document provides a structured overview of the concepts, components, and implementation plan for the project on parallelizing blockchain computations using C++, MPI, and a sharded PBFT architecture. **This version is updated to reflect the final implemented architecture.**

---

## Core Concepts & Definitions

-   **Blockchain:** A distributed, immutable ledger. In this project, it is implemented as a chain of `MacroBlock`s. (Priority: **High**)
-   **Sharding:** The core parallelization strategy. The network is partitioned into smaller `Shard` committees, each processing a subset of transactions in parallel. (Priority: **High**)
-   **Shard (Committee):** A subset of MPI processes assigned to a specific shard, responsible for running PBFT to produce a `MicroBlock`. (Priority: **High**)
-   **Final Committee:** A dedicated committee responsible for collecting `MicroBlock`s from all shards and assembling them into a `MacroBlock`. (Priority: **High**)
-   **PBFT (Practical Byzantine Fault Tolerance):** The consensus algorithm used *within* each shard to validate transactions and agree on a `MicroBlock`. (Priority: **High**)
-   **Block Hierarchy:** The project uses a two-tiered block structure:
    -   **`MicroBlock`**: A block containing transactions, produced and signed by a single shard.
    -   **`MacroBlock`**: A block that aggregates `MicroBlock`s from all shards, forming the main chain. It contains `MicroBlock` hashes, not raw transactions.
-   **Transaction (`Transaction`):** The fundamental unit of work. A serializable data structure representing a transfer of value or data. (Priority: **High**)
-   **Node (`Node`):** Represents a single MPI process, holding its rank, shard assignment, and role (e.g., `SHARD_LEADER`). (Priority: **High**)
-   **State (`State`):** The "world state" of the blockchain, representing the cumulative result of all executed transactions. (Priority: **Medium**)
-   **Mempool (`Mempool`):** A data structure within each node or shard that holds unconfirmed transactions waiting to be processed. (Priority: **Medium**)

---

## Algorithms & Protocols

-   **Intra-Shard Consensus (PBFT):** The three-phase (`pre-prepare`, `prepare`, `commit`) protocol implemented within each shard's MPI communicator to agree on a `MicroBlock`. (Priority: **High**)
-   **Block Aggregation:** The protocol used by the `FinalCommittee`. Shard leaders `MPI_Send` their `MicroBlock`s to the final committee leader, which then assembles them into a `MacroBlock`. (Priority: **High**)
-   **State Machine Replication:** The underlying model where the blockchain ledger is replicated across nodes, with PBFT serving as the replication protocol. (Priority: **Medium**)

---

## System Components / Modules (`src/` and `include/`)

-   **`main.cpp`**: The orchestrator. Initializes MPI, parses `Config`, partitions nodes into shards and a final committee using `MPI_Comm_split`, distributes transactions, and drives the simulation.
-   **`core/`**:
    -   **`Transaction`**: Defines the transaction structure and serialization.
    -   **`Node`**: Represents a network participant.
    -   **`blocks/`**: Implements the `Block`, `BlockHeader`, `MicroBlock`, and `MacroBlock` classes.
    -   **`state/`**: Manages the global state, including `Genesis` block creation.
    -   **`mempool/`**: Manages the pool of pending transactions.
    -   **`Blockchain`**: Manages the final chain of `MacroBlock`s.
-   **`consensus/`**:
    -   **`PBFT`**: The core PBFT engine that operates within a shard.
    -   **`PBFTMessages`**: Defines the messages exchanged during the PBFT protocol.
-   **`network/`**:
    -   **`Shard`**: Manages a shard committee, its mempool, and its PBFT process.
    -   **`FinalCommittee`**: Manages the final committee's task of collecting and assembling blocks.
    -   **`MPIWrapper`**: A utility to simplify MPI communication patterns.
-   **`util/`**: Contains helper modules like `Config`, `Crypto`, `Logger`, `Serializer`, and `Timer`.

---

## Data Flow Diagram

```
                               +--------------------+
[Root Process] --(Scatter Tx)-->| Shard 1 (PBFT)     | --(Send MicroBlock)--> +-----------------+
                               | - Processes Tx     |                        |                 |
                               | - Creates MicroBlk |                        |                 |
                               +--------------------+                        |                 |
                               +--------------------+                        | Final Committee | --(Append)--> [Global Blockchain]
[Root Process] --(Scatter Tx)-->| Shard 2 (PBFT)     | --(Send MicroBlock)--> | - Collects      |   (Chain of MacroBlocks)
                               | - Processes Tx     |                        | - Assembles     |
                               | - Creates MicroBlk |                        |   MacroBlock    |
                               +--------------------+                        |                 |
                                     ...                                     +-----------------+
                               +--------------------+
[Root Process] --(Scatter Tx)-->| Shard N (PBFT)     | --(Send MicroBlock)-->
                               | - Processes Tx     |
                               | - Creates MicroBlk |
                               +--------------------+
```
1.  A root process generates and distributes transactions to multiple **Shards**.
2.  Each Shard runs **PBFT consensus** in parallel on its transaction subset, producing a **`MicroBlock`**.
3.  Shard leaders send their `MicroBlock` to the **Final Committee**.
4.  The Final Committee leader gathers all `MicroBlock`s, creates a **`MacroBlock`** containing their hashes, and appends it to the global blockchain.

---

## Performance Metrics & Experiments

-   **Baseline (Serial) Model:** A non-sharded version (1 shard) where all nodes form a single committee.
-   **Parallel (Sharded) Model:** The full sharded implementation, tested with a varying number of shards (e.g., 2, 4, 8, 16).
-   **Throughput (TPS):** Total finalized transactions per second.
-   **Latency:** Average time from transaction submission to finalization in a `MacroBlock`.
-   **Speedup:** `Time_Serial / Time_Parallel`. The primary measure of performance improvement.
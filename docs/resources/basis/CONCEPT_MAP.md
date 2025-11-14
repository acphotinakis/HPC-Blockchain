# Project Concept Map: Parallelizing Blockchain Computations

This document provides a structured overview of the concepts, components, and implementation plan for the project on parallelizing blockchain computations using C++, MPI, and a sharded PBFT architecture.

---

## Core Concepts & Definitions

- **Blockchain:** A distributed, immutable ledger structured as a chain of cryptographically linked blocks. The project aims to parallelize its traditionally sequential nature. (Priority: **High**)
- **Sharding:** A database partitioning technique that breaks the network and transaction load into smaller, independent parts (shards) to enable parallel processing and improve scalability. (Priority: **High**)
- **Committee (Shard):** A subset of network nodes (MPI processes) assigned to a specific shard, responsible for reaching consensus on its own transaction pool. (Priority: **High**)
- **Byzantine Fault:** A type of fault where a component can behave arbitrarily or maliciously. The system must tolerate a certain number of such faults to ensure correctness. (Priority: **High**)
- **Consensus:** The process by which nodes in a distributed system agree on a single, consistent state, such as the order of transactions in a block. (Priority: **High**)
- **Final Committee:** A dedicated group of nodes responsible for aggregating validated blocks from all shards to form the final, global blockchain. (Priority: **Medium**)
- **Transaction Throughput (TPS):** The rate at which valid transactions are committed by the network per second. This is a primary metric for measuring performance improvement. (Priority: **High**)
- **Latency:** The time it takes for a submitted transaction to be finalized on the global blockchain. This is a key performance metric expected to decrease with sharding. (Priority: **High**)

---

## Algorithms & Protocols

- **Practical Byzantine Fault Tolerance (PBFT):** A consensus algorithm that provides safety and liveness in asynchronous systems with Byzantine nodes. It will be implemented for intra-shard consensus, involving a three-phase (`pre-prepare`, `prepare`, `commit`) protocol. (Priority: **High**)
- **State Machine Replication:** The foundational approach of this project, where a service (the blockchain ledger) is replicated across multiple nodes to provide fault tolerance. PBFT is the chosen replication protocol. (Priority: **Medium**)
- **Cross-Shard Consensus Protocol:** A protocol, which is a key research and implementation challenge of this project, to ensure atomic commitment of transactions that span multiple shards. (Priority: **Medium**)

---

## System Components / Modules

- **Transaction Generator:** A component to create a large pool of mock transactions to drive the simulation and measure performance under load. (Priority: **High**)
- **Node (`node.h`/`node.cpp`):** A class representing a single MPI process in the network, holding its state, rank, and role (e.g., shard member, shard leader, final committee member). (Priority: **High**)
- **Transaction (`transaction.h`/`transaction.cpp`):** A data structure for a single transaction. It must be serializable to be passed between MPI processes. (Priority: **High**)
- **Shard Manager (`shard.h`/`shard.cpp`):** A module to manage the nodes within a shard, its local transaction pool, and orchestrate the PBFT consensus process. (Priority: **High**)
- **PBFT Module (`pbft.h`/`pbft.cpp`):** An implementation of the PBFT algorithm that operates within a specific MPI communicator to validate a block of transactions. (Priority: **High**)
- **Blockchain (`blockchain.h`/`blockchain.cpp`):** A class that manages the final, aggregated blockchain data structure, assembled and maintained by the final committee. (Priority: **Medium**)
- **MPI Communicator Manager:** Logic within `main.cpp` to partition the global set of MPI processes into communicators for each shard and the final committee using `MPI_Comm_split`. (Priority: **High**)

---

## Data & File Artifacts

- **`@resources/basis/PROPOSAL.md`:** The main document defining the project's problem, solution (Sharding + PBFT), methodology (C++/MPI), and performance goals.
- **`@resources/basis/STRUCTURE.md`:** Defines the purpose of each source file and directory, providing a blueprint for the codebase.
- **`@README.md`:** A high-level summary of the project, its goals, and instructions for building and running it.
- **`@IMPLEMENTATION_PLAN.md`:** A phased development plan, detailing the steps from foundational components to the final parallel model and testing.
- **`@resources/overview_project/Blockchain.pdf`:** Course slides providing academic context on blockchain scalability issues and introducing sharding as a solution.
- **`@scripts/run_experiment.sh`:** A script to automate running simulations with different parameters (e.g., number of shards) on the target cluster.
- **`@scripts/plot_results.py`:** A script to parse experiment output and generate performance graphs (TPS, Latency, Speedup).
- **`@docs/PERFORMANCE.md`:** The designated file for the final performance analysis, graphs, and interpretation of results.
- **`@Makefile`:** The build script for compiling the C++/MPI source code.

---

## Implementation Notes (C++ & MPI)

- **Node Representation:** Each MPI process will simulate a single node in the blockchain network. (Priority: **High**)
- **Shard Communicators:** Shards will be implemented as separate MPI communicators created with `MPI_Comm_split`. This isolates communication, enabling parallel consensus. (Priority: **High**)
- **Transaction Distribution:** The root process will distribute transactions to shard leaders using `MPI_Scatter` or point-to-point `MPI_Send`. (Priority: **High**)
- **Intra-Shard Consensus (PBFT):** The PBFT phases will be implemented using MPI collectives like `MPI_Bcast` (for proposals) and `MPI_Allgather` (to collect votes). (Priority: **High**)
- **Final Aggregation:** The final committee will use `MPI_Gather` or repeated `MPI_Recv` calls to collect validated block headers from shard leaders. (Priority: **Medium**)
- **Serialization:** `Transaction` objects and other data will require custom serialization/deserialization functions to be packed into byte buffers for MPI communication. (Priority: **High**)

---

## Performance Metrics & Experiments

- **Baseline (Serial) Model:** A non-sharded version where all nodes form a single committee. This provides the baseline for calculating speedup. (Priority: **High**)
- **Parallel (Sharded) Model:** The full sharded implementation, to be tested with a varying number of shards (e.g., 2, 4, 8, 16). (Priority: **High**)
- **Throughput (TPS):** Measure total finalized transactions per second.
- **Latency:** Measure the average time from transaction submission to finalization.
- **Speedup:** Calculate as `Time_Serial / Time_Parallel`. This is the primary measure of performance improvement.

---

## Risks, Assumptions & Open Questions

- **Cross-Shard Transaction Atomicity:** The protocol for handling transactions that affect multiple shards is not fully defined and is a primary challenge. (Priority: **Medium**)
- **Faulty Final Committee:** The model does not specify how to handle Byzantine failures within the final committee itself. (Priority: **Low**)
- **Transaction-to-Shard Mapping:** The logic for assigning transactions to shards (e.g., based on sender ID) needs to be defined and implemented. (Priority: **Medium**)
- **MPI Communication Overhead:** Message complexity could become a bottleneck as the number of shards increases, limiting scalability. (Priority: **Medium**)
- **PBFT View Changes:** The implementation may initially assume a non-faulty leader within each shard, as the view-change protocol is a secondary goal. (Priority: **Low**)

---

## High-Level Relationship Diagram

```
                               +-----------------+
[Root Process] --(Scatter Tx)-->|   Shard 1 (PBFT)|--+
                               +-----------------+  |
                               +-----------------+  |
[Root Process] --(Scatter Tx)-->|   Shard 2 (PBFT)|--+--(Gather Blocks)-->+----------------+--(Append)-->[Global Blockchain]
                               +-----------------+  |                     | Final Committee|
                                     ...            |                     +----------------+
                               +-----------------+  |
[Root Process] --(Scatter Tx)-->|   Shard N (PBFT)|--+
                               +-----------------+
```
- A root process generates and distributes transactions to multiple Shards.
- Each Shard runs PBFT consensus in parallel on its transaction subset.
- Shard leaders send their validated blocks to a Final Committee, which aggregates them into the final chain.

---

## Top 5 Next Actionable Items

1.  **Implement Basic Data Structures:** Create the `Transaction` and `Node` classes with necessary serialization methods.
2.  **Implement Baseline Serial Model:** Build a single-committee PBFT implementation using `MPI_COMM_WORLD` to establish a performance baseline.
3.  **Implement MPI Partitioning:** Write the logic to partition nodes into shard-specific communicators using `MPI_Comm_split`.
4.  **Develop the Shard Manager:** Create the `Shard` class to manage its transaction pool and orchestrate the PBFT process within its communicator.
5.  **Create Initial PBFT Test:** Develop a test suite (`test_pbft.cpp`) to verify the correctness of the PBFT implementation with a small number of processes.

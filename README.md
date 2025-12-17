# Parallelizing Blockchain Computations via Sharding (using PBFT and MPI)

## TL;DR

This project implements a **sharded, PBFT-based blockchain simulator** in **C++ using MPI**
to demonstrate how **parallel consensus** improves transaction throughput and latency.

We compare:
- a **serial PBFT baseline**
- against a **parallel sharded architecture**

and show **near-linear throughput scaling** as shard count increases.


## Project Structure

```
.
├── Makefile
├── CMakeLists.txt
├───.clang-format
├───.clangd
├───.geminiignore
├───.gitignore
├── README.md
├── simulation.py
├── walk_thru.py
├── build/
├── docs/
│   ├── ... (project documentation)
├── include/
│   └── sbmpi/
│       ├── consensus/
│       │   ├── pbft_messages.h
│       │   └── pbft.h
│       ├── core/
│       │   ├── blockchain.h
│       │   ├── node.h
│       │   ├── blocks/
│       │   │   ├── block.h
│       │   │   ├── blockheader.h
│       │   │   ├── macro_block.h
│       │   │   └── micro_block.h
│       │   ├── mempool/
│       │   │   └── mempool.h
│       │   └── state/
│       │       ├── genesis.h
│       │       ├── state.h
│       │       ├── transaction.h
│       │       └── wallet.h
│       ├── network/
│       │   ├── cross_shard.h
│       │   ├── mpi_wrapper.h
│       │   ├── shard.h
│       │   └── committee/
│       │       ├── committee.h
│       │       └── final_committee.h
│       └── util/
│           ├── config.h
│           ├── crypto.h
│           ├── errors.h
│           ├── generator.h
│           ├── logging.h
│           ├── metrics.h
│           ├── serialization.h
│           ├── threadpool.h
│           └── timer.h
├── json/
│   └── single_include/nlohmann/json.hpp
├── scripts/
│   ├── run_experiment.sh
│   └── plot_results.py
├── secp256k1/
│   └── ... (vendored crypto library)
├── src/
│   ├── main.cpp
│   ├── consensus/
│   │   ├── pbft_messages.cpp
│   │   └── pbft.cpp
│   ├── core/
│   │   ├── blockchain.cpp
│   │   ├── node.cpp
│   │   ├── blocks/
│   │   │   ├── ... (block implementations)
│   │   ├── mempool/
│   │   │   └── mempool.cpp
│   │   └── state/
│   │       ├── ... (state implementations)
│   ├── network/
│   │   ├── ... (network implementations)
│   └── util/
│       ├── ... (utility implementations)
└── tests/
    ├── test_pbft.cpp
    └── test_sharding.cpp
```
## Overview

This project addresses the critical scalability bottleneck in traditional blockchain architectures by implementing a parallel blockchain algorithm using sharding. Our goal is to demonstrate significant performance improvements in transaction throughput and latency by leveraging parallel computing principles.

## Problem Statement

Traditional blockchains process transactions sequentially, leading to low transaction throughput and high latency. This limits their applicability for large-scale use cases. This project aims to overcome this limitation by introducing parallelism into the consensus mechanism.

## Proposed Solution: Sharding with PBFT

Our solution employs a sharding method to parallelize blockchain computations. The core components include:

-   **Committees (Shards):** The network of nodes is partitioned into several independent "committees" or shards.
-   **Transaction Partitioning:** Incoming transactions are divided among these committees, with each committee processing its own subset of transactions.
-   **Intra-Committee Consensus (PBFT):** Within each shard, the Practical Byzantine Fault Tolerance (PBFT) algorithm is used to achieve consensus on a block of transactions.
-   **Parallel Block Creation:** Multiple transaction blocks are created and validated in parallel across different committees.
-   **Final Committee:** A dedicated "final committee" aggregates and verifies the validated blocks from each shard, assembling the final, ordered blockchain.

This approach enables multiple committees to reach consensus in parallel, significantly increasing transaction throughput.

### Architectural Deep Dive & Simulation Flow

This project simulates a sharded blockchain network using C++ and the Message Passing Interface (MPI). The architecture is designed to clearly separate concerns and demonstrate the performance benefits of parallel transaction processing.

#### Core Components & Node Roles

The network is composed of nodes, each an independent MPI process. Nodes are assigned one of three roles:

1.  **Shard Member:** The workhorse of the network. These nodes are grouped into **Shards** (or **Committees**) and are responsible for processing a subset of all transactions. They participate in the intra-shard consensus protocol.
2.  **Shard Leader:** One node within each shard is designated as the leader. The leader receives transactions from the main process, proposes new `MicroBlock`s to its fellow shard members, and initiates the consensus process.
3.  **Final Committee Member:** A small, dedicated group of nodes responsible for finalizing the blockchain. The leader of this committee gathers the `MicroBlock`s that have achieved consensus within each shard and assembles them into a definitive `MacroBlock`, which is then added to the global blockchain.

This hierarchical structure is achieved by splitting `MPI_COMM_WORLD` into separate communicators for each shard and one for the final committee, allowing for clean and efficient intra-group communication.

#### Simulation Lifecycle

A typical simulation run proceeds through the following phases:

1.  **Initialization & Node Assignment:**
    -   MPI is initialized, and each process (node) determines its global rank and the total number of processes.
    -   Based on the `--shards` parameter, the `determineNodeAssignment` function assigns a role (`Shard Member`, `Shard Leader`, `Final Committee Member`) and a group ID (`shard_color`) to each node.
    -   MPI communicators are split, creating isolated communication groups for each shard and the final committee.

2.  **Transaction Generation & Distribution:**
    -   The root process (rank 0) generates a configurable number of mock transactions using `generateMockTransactions`. This function also handles signing transactions with mock wallets and can inject faulty transactions for testing resilience.
    -   These transactions are partitioned and sent from the root process directly to the designated **Shard Leader** for each shard.

3.  **Intra-Shard Consensus (PBFT):**
    -   Inside each shard, the nodes execute the Practical Byzantine Fault Tolerance (PBFT) protocol to agree on the next block of transactions.
    -   The Shard Leader proposes a `MicroBlock`.
    -   Shard Members receive the proposal, validate the transactions within it (in parallel using OpenMP), and then proceed through the PBFT phases: **Pre-Prepare**, **Prepare**, and **Commit**.
    -   A block is considered committed once a quorum of `2f+1` nodes (where `f` is the number of faulty nodes the shard can tolerate) has broadcasted their commitment.
    -   The output of this phase is a `MicroBlock` that has been validated and agreed upon by its shard.

4.  **Aggregation and Finalization:**
    -   Each Shard Leader sends its newly created `MicroBlock` to the leader of the **Final Committee**.
    -   The Final Committee leader collects one `MicroBlock` from each shard.
    -   It then assembles these into a single `MacroBlock`, which contains the hashes of all included microblocks and an aggregated list of all their transactions.
    -   This `MacroBlock` is added to the shared `Blockchain` data structure, finalizing the state of all transactions from that round.

5.  **Metrics and Analysis:**
    -   Throughout the simulation, `Timer` and `Metrics` utilities record performance data, including total execution time, throughput (transactions per second), block creation times, and consensus duration for each shard.
    -   This data is written to CSV files in the `/metrics` directory, which can be visualized using the `scripts/plot_results.py` script.

## Implementation Details

-   **Language:** C++ (for performance and low-level control).
-   **Parallel Paradigm:** Message Passing Interface (MPI), ideal for simulating a distributed, message-passing-based network.
-   **Simulation Design:** We will compare a **Baseline (Serial) Model** (a single-committee PBFT implementation) against a **Parallel (Sharded) Model** (the full hierarchical implementation with multiple shards and a final committee) under identical transaction loads.
-   **Hardware:** The project is designed to run on parallel systems like the CS department's `kraken` cluster, utilizing its high core count for simulating a large-scale network.

## Performance Measurement

Our methodology focuses on quantifying the performance gains of the sharded model. Key metrics include:

-   **Throughput (TPS):** Total transactions finalized per second. Expected to scale (near) linearly with the number of shards.
-   **Latency (Time):** Average time from transaction submission to final confirmation. Expected to decrease with increased shard count.
-   **Speedup:** Ratio of serial execution time to parallel execution time. Expected to be greater than 1 and increase with the number of shards.

The evaluation process involves running both models, measuring these metrics, and graphing the results to demonstrate scalability and efficiency.

## Project Structure

The project is organized into several key directories:

-   `/src`: C++ source code for blockchain logic, PBFT, sharding, nodes, and transactions.
-   `/include`: C++ header files corresponding to the source code.
-   `/build`: Compiled executables and object files.
-   `/tests`: Unit and integration tests for core components.
-   `/metrics`: Output data and graphs from performance experiments.
-   `/scripts`: Helper scripts for running experiments and plotting results.
-   `/docs`: Supplementary project documentation, including `PERFORMANCE.md`.

For a detailed breakdown of each directory and file, please refer to `STRUCTURE.md`.

## Getting Started

1.  **Compilation:** Use the provided `Makefile` to compile the project.
    ```bash
    make
    ```
    This will generate executables in the `/build` directory.

2.  **Running Experiments:** Utilize the scripts in the `/scripts` directory to execute simulations and gather performance data.
    ```bash
    ./scripts/run_experiment.sh <parameters>
    ```

3.  **Analyzing Results:** Use the plotting script to visualize the performance data stored in `/metrics`.
    ```bash
    python scripts/plot_results.py
    ```

## References

- "A Survey of Blockchain Consensus Protocols." (20XX, Author(s) Unknown).
- "An efficient sharding consensus algorithm for consortium chains." (20XX, Author(s) Unknown).
- "Analyzing fault aware collective performance in a process fault tolerant MPI." (20XX, Author(s) Unknown).
- Castro, M., & Liskov, B. (1999). *"Practical Byzantine Fault Tolerance"*. Proceedings of the Third Symposium on Operating Systems Design and Implementation.
- Kumar, M. (2025). *"Parallelism and Blockchain"* (Blockchain.pdf). CSCI 654 Lecture Slides.
- Luu, L., et al. (2016). *"A Secure Sharding Protocol for Open Blockchains (Elastico)"*. Proceedings of the 2016 ACM SIGSAC Conference on Computer and Communications Security.
- "On Sharding Permissioned Blockchains." (20XX, Author(s) Unknown).
- "Reaching Consensus in the Byzantine Empire - A Comprehensive Survey." (20XX, Author(s) Unknown).
- "Survey of Sharding in Blockchains." (20XX, Author(s) Unknown).
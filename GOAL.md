# Project Goals: Parallelizing Blockchain Computations

This document outlines the primary, architectural, performance, and technical goals of this simulation project. The central aim is to research, implement, and validate a method for scaling blockchain performance through parallel computing.

## 1. Primary Goal: Demonstrate Scalability via Sharding

The single most important goal of this project is to **quantitatively prove that a sharded blockchain architecture can achieve significantly higher transaction throughput and lower latency compared to a traditional, sequential model.**

This will be achieved by:
-   **Building Two Models:**
    -   A **Baseline (Serial) Model** simulating a single, large consensus committee that processes all transactions sequentially.
    -   A **Parallel (Sharded) Model** that partitions the network into multiple smaller committees (shards), each processing a subset of transactions in parallel.
-   **Conducting Rigorous Experiments:** Running both models under identical, reproducible transaction loads.
-   **Measuring and Comparing Performance:** Systematically collecting and analyzing key performance indicators to highlight the speedup and efficiency gains of the parallel design.

---

## 2. Architectural & Functional Goals

To support the primary goal, the project aims to implement a well-defined, hierarchical blockchain architecture with the following functional components:

-   **Node Partitioning:** Dynamically assign MPI processes to distinct roles (`Final Committee`, `Shard Leader`, `Shard Member`) and partition them into logical communication groups (shards).
-   **Transaction Distribution:** Implement a mechanism for the root process to generate a large, deterministic set of transactions and distribute them across the designated shards.
-   **Intra-Shard Consensus:** Within each shard, correctly implement the **Practical Byzantine Fault Tolerance (PBFT)** protocol to allow shard members to securely agree on a `MicroBlock` of transactions. This includes handling the multi-phase message exchange (Pre-Prepare, Prepare, Commit) and ensuring a `2f+1` quorum is reached.
-   **Parallel Block Creation:** Enable multiple shards to run the PBFT consensus protocol simultaneously, each on its own subset of transactions.
-   **Block Aggregation:** Implement a `FinalCommittee` responsible for collecting the validated `MicroBlock`s from all shard leaders.
-   **Chain Finalization:** The `FinalCommittee` must assemble the collected `MicroBlock`s into a final, ordered `MacroBlock` and append it to a global `Blockchain` data structure, thus finalizing the transactions from that round.
-   **Fault Injection and Resilience:** The system should be robust enough to handle faulty (Byzantine) behavior. The transaction generator includes a feature to create invalid transactions, and the PBFT implementation is designed to tolerate them, providing a basic framework for testing fault tolerance.

---

## 3. Performance & Measurement Goals

The project aims to achieve and demonstrate specific, measurable performance improvements.

-   **Measure Throughput (Transactions Per Second):** The primary success metric. The goal is to show that throughput (TPS) increases as the number of shards increases, ideally in a near-linear fashion.
-   **Measure Latency:** Track the total time from the start of the simulation to the finalization of the `MacroBlock`. The goal is to show that for a fixed total number of transactions, increasing the number of shards decreases the overall processing time.
-   **Calculate Speedup:** Formally calculate the speedup (`Serial_Execution_Time` / `Parallel_Execution_Time`) and demonstrate that it is significantly greater than 1 and grows with the number of shards.
-   **Comprehensive Metrics Collection:** Instrument the code to log detailed performance data for each simulation run, including:
    -   Overall simulation time.
    -   Consensus time per shard.
    -   Number of messages exchanged during consensus.
    -   Block creation and finalization times.
-   **Data Visualization:** Provide a Python script (`scripts/plot_results.py`) to parse the generated metrics (from CSV files) and create clear, insightful plots that visually confirm the performance gains.

---

## 4. Technical & Implementation Goals

The project is built on specific technical choices to meet the simulation's requirements.

-   **Utilize C++:** Leverage C++ for its high performance, low-level control over memory, and suitability for complex, computationally intensive simulations.
-   **Leverage MPI:** Use the Message Passing Interface (MPI) as the parallel programming paradigm to accurately simulate a distributed network of nodes where communication happens via explicit message passing.
-   **Clean, Modular Codebase:** Adhere to a clean, well-documented, and modular project structure (`include/`, `src/`, `core/`, `network/`, `consensus/`, etc.) to ensure the codebase is understandable, maintainable, and extensible.
-   **Cryptographic Primitives:** Integrate standard cryptographic libraries (`secp256k1`, `OpenSSL`) to implement essential blockchain features like digital signatures (ECDSA) and hashing (Keccak-256), adding a layer of realism to the simulation's transactions.
-   **Repeatable Experiments:** Ensure that experiments are deterministic and repeatable by using a fixed seed for transaction generation, allowing for fair comparisons between different simulation configurations.

# Implementation Plan: Parallelizing Blockchain Computations

This document outlines a detailed implementation plan for the C++/MPI project aimed at parallelizing blockchain computations using sharding and the Practical Byzantine Fault Tolerance (PBFT) consensus algorithm. The plan is derived from the project proposal and structure documents.

---

## 1. Project Overview and Goal

The primary objective is to build a C++/MPI simulation that demonstrates a significant performance improvement in blockchain transaction processing. We will compare a traditional, sequential blockchain model against a parallel, sharded model. The implementation will follow the structure defined in `resources/basis/STRUCTURE.md` and will be executed on the `kraken` cluster.

The core of the project is to show that by partitioning the network and transaction load (sharding), we can run multiple consensus instances (PBFT) in parallel, thereby increasing throughput and reducing latency.

---

## 2. Phase 1: Foundational Component Implementation

This phase focuses on creating the basic data structures and classes that will be used throughout the simulation. These components must be robust before moving to the consensus and networking logic.

### **`transaction.h` / `transaction.cpp`**
-   **Purpose:** Define the fundamental unit of work in the blockchain.
-   **Implementation Details:**
    -   Create a `Transaction` class or struct.
    -   It will contain basic information such as a unique ID, a sender ID, a receiver ID, and a value or data payload.
    -   Implement serialization and deserialization methods. This is critical for sending transaction data between MPI processes. We will need a way to pack a `Transaction` object into a byte buffer for `MPI_Send` and unpack it after `MPI_Recv`.

### **`node.h` / `node.cpp`**
-   **Purpose:** Represent a single participant (a process) in the network.
-   **Implementation Details:**
    -   Create a `Node` class.
    -   Each MPI process will instantiate a `Node` object.
    -   The class will store the node's state, including its rank in the global communicator, its rank within its shard communicator, and its assigned role (e.g., leader, replica, member of the final committee).
    -   It will not contain complex communication logic itself but will hold the identifiers necessary for other components to perform communication on its behalf.

---

## 3. Phase 2: Baseline Model - Single-Committee PBFT

This phase implements the non-parallel baseline for performance comparison. All nodes will belong to a single large committee that processes all transactions sequentially.

### **`pbft.h` / `pbft.cpp`**
-   **Purpose:** Implement the core PBFT consensus algorithm.
-   **Implementation Details:**
    -   This will be the most complex component. It must manage the state machine of the PBFT protocol: `pre-prepare`, `prepare`, and `commit`.
    -   A `PBFT` class will be designed to operate within a specific `MPI_Comm`.
    -   **Pre-prepare:** The leader node (e.g., rank 0 within the committee) will broadcast a proposed block of transactions to all other nodes (replicas) using `MPI_Bcast`.
    -   **Prepare:** Upon receiving a `pre-prepare` message, replicas validate the proposal and broadcast a `prepare` message to all other nodes in the committee. This will likely involve an `MPI_Allgather` to collect all `prepare` messages.
    -   **Commit:** Once a node receives a sufficient number of `prepare` messages (2f, where f is the number of faulty nodes), it broadcasts a `commit` message. An `MPI_Allgather` will be used again to confirm that a quorum of nodes has committed.
    -   The logic must handle the view-change protocol in case a leader is faulty, although this can be a secondary goal after the primary path is working.
    -   This component will be heavily reliant on MPI point-to-point and collective communication.

### **`blockchain.h` / `blockchain.cpp`**
-   **Purpose:** Define the structure of the blockchain itself.
-   **Implementation Details:**
    -   Create a `Block` class containing a block header (previous hash, timestamp, block number) and a list of `Transaction` objects.
    -   Create a `Blockchain` class that manages a `std::vector<Block>`.
    -   It will include a method `addBlock()`, which validates a new block (e.g., checks the hash linkage) and appends it to the chain.
    -   For the baseline model, this will be managed by the single committee.

### **`main.cpp` (Initial Version)**
-   **Purpose:** Orchestrate the baseline simulation.
-   **Implementation Details:**
    -   Initialize MPI.
    -   All processes will operate within the `MPI_COMM_WORLD` communicator.
    -   The root process (rank 0) will generate a large set of mock transactions.
    -   These transactions will be broadcast to all nodes.
    -   Instantiate the `PBFT` class, passing `MPI_COMM_WORLD` to it.
    -   Execute the PBFT consensus process on the entire transaction set.
    -   Once consensus is reached, the leader will form a block and add it to its instance of the `Blockchain`.
    -   Measure and record the total time taken from transaction generation to final block creation.

---

## 4. Phase 3: Parallel Model - Sharded PBFT Implementation

This phase introduces the core parallelization logic by extending the baseline model with sharding.

### **`shard.h` / `shard.cpp`**
-   **Purpose:** Manage a single shard (committee).
-   **Implementation Details:**
    -   Create a `Shard` class.
    -   This class will hold the shard-specific `MPI_Comm`.
    -   It will contain a list of the global ranks of the nodes belonging to it.
    -   It will be responsible for receiving its subset of transactions from the main process.
    -   Its primary role is to instantiate and run the `PBFT` consensus algorithm on its local transaction pool, using its own communicator.
    -   Upon completion, the leader of the shard will hold a validated block (or block header).

### **`main.cpp` (Sharded Version)**
-   **Purpose:** Orchestrate the full, parallel sharded simulation.
-   **Implementation Details:**
    -   **MPI Setup:** After initializing MPI, the primary task is to partition the nodes. The code will divide the set of all processes into `N` shards and one final committee. This will be done using `MPI_Comm_split`. Each process will get a new rank and communicator based on its assigned shard.
    -   **Transaction Distribution:** The root process (global rank 0) will generate all transactions and then use `MPI_Scatter` or multiple `MPI_Send` calls to distribute distinct subsets of these transactions to the leader of each shard.
    -   **Parallel Consensus:** Each shard will independently and concurrently execute the PBFT algorithm on its transaction set. Since they operate in separate communicators, their communications will not interfere.
    -   **Aggregation:** Once a shard's consensus is complete, its leader will send the resulting validated block (or block header) to the final committee using `MPI_Send`.
    -   **Finalization:** The final committee will receive blocks from all shard leaders (e.g., using `MPI_Gather` or repeated `MPI_Recv` calls). It will then verify the signatures/proofs from each shard and assemble these micro-blocks into the final, global blockchain.
    -   The total time for this entire process will be measured for comparison with the baseline.

---

## 5. Phase 4: Testing and Verification

Correctness is paramount. Testing will be done in parallel with development.

### **`tests/test_pbft.cpp`**
-   **Purpose:** Unit test the PBFT implementation in isolation.
-   **Implementation Details:**
    -   Create a test executable that runs a small number of processes.
    -   It will test the PBFT logic under ideal conditions (no failures).
    -   It will also simulate faulty nodes (nodes that do not respond or send conflicting messages) to ensure the protocol can still reach consensus if `n >= 3f + 1`.

### **`tests/test_sharding.cpp`**
-   **Purpose:** Verify the node partitioning and transaction distribution logic.
-   **Implementation Details:**
    -   Create a test executable that initializes MPI.
    -   It will perform the `MPI_Comm_split` operation and have each process print its new rank and communicator ID to verify the partitioning is correct.
    -   It will test the `MPI_Scatter` of transactions to ensure each shard receives the correct, non-overlapping data.

---

## 6. Phase 5: Experimentation and Performance Analysis

This phase focuses on running the simulation and producing the results required by the proposal.

### **`scripts/run_experiment.sh`**
-   **Purpose:** Automate the execution of experiments on `kraken`.
-   **Implementation Details:**
    -   The script will be configurable, taking the total number of nodes, the number of shards, and the number of transactions as command-line arguments.
    -   It will invoke `mpirun` with the correct parameters (e.g., `mpirun -np 64 ./build/main --shards 8 --transactions 10000`).
    -   It will redirect the timing output from the simulation into a CSV file in the `/results` directory.
    -   The script will loop through different shard configurations (e.g., 1 (baseline), 2, 4, 8, 16) to gather comprehensive data.

### **`scripts/plot_results.py`**
-   **Purpose:** Analyze and visualize the performance data.
-   **Implementation Details:**
    -   This Python script will use libraries like `pandas` and `matplotlib`.
    -   It will read the CSV files from the `/results` directory.
    -   It will calculate the key metrics: Throughput (Transactions Per Second), Latency, and Speedup (`Time_Serial / Time_Parallel`).
    -   It will generate the graphs specified in the proposal: Throughput vs. Shards, Latency vs. Shards, and Speedup vs. Shards. These graphs will be saved as image files in `/results`.

### **`Makefile`**
-   **Purpose:** Automate the build process.
-   **Implementation Details:**
    -   It will have rules to compile all `.cpp` files in `/src` and `/tests` using `mpic++`.
    -   It will link the object files to create the final executables (`main`, `test_pbft`, `test_sharding`) in the `/build` directory.
    -   It will include a `clean` rule to remove the `/build` directory.

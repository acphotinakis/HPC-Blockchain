# Project Structure: Parallelizing Blockchain Computations

This document outlines the directory and file structure for the C++/MPI implementation of a sharded blockchain using PBFT. The structure reflects a significant evolution from the initial design, featuring a more modular and hierarchical component architecture.

---

## Core Directories

-   **`/src`**: Contains all C++ source code (`.cpp` files).
-   **`/include`**: Contains all C++ header files (`.h` files) that define the interfaces for the components implemented in `/src`.
-   **`/build`**: Destination for all compiled binaries and object files.
-   **`/tests`**: Contains source files for unit and integration tests.
-   **`/results`**: Stores output data from performance experiments (e.g., CSV files, graphs).
-   **`/scripts`**: Holds helper scripts for automating experiments and plotting results.
-   **`/docs`**: Contains supplementary project documentation.
-   **`/resources`**: Contains research papers and other reference materials.

---

## Detailed Source & Include Structure

### `/include/sbmpi/` & `/src/`

#### `core`: Core Blockchain Mechanics
-   **`core/blocks/`**: Defines the different types of blocks.
    -   `block.h`/`.cpp`: An abstract base class for all block types.
    -   `blockheader.h`/`.cpp`: Defines the header component of a block.
    -   `micro_block.h`/`.cpp`: A concrete block created by a shard committee.
    -   `macro_block.h`/`.cpp`: A final block that aggregates `MicroBlock`s.
-   **`core/mempool/`**:
    -   `mempool.h`/`.cpp`: Implements the transaction pool for nodes.
-   **`core/state/`**: Manages the blockchain's world state.
    -   `state.h`/`.cpp`: Manages state transitions and data.
    -   `genesis.h`/`.cpp`: Logic for creating the first block of the chain.
-   **`blockchain.h`/`.cpp`**: Manages the overall chain of `MacroBlock`s.
-   **`node.h`/`.cpp`**: Represents a single MPI process (a node) in the network.
-   **`transaction.h`/`.cpp`**: Defines the `Transaction` data structure and its serialization.

#### `consensus`: Consensus Algorithm
-   **`pbft.h`/`.cpp`**: The core implementation of the PBFT consensus algorithm.
-   **`pbft_messages.h`/`.cpp`**: Defines the structures and serialization for messages used in PBFT (e.g., Pre-Prepare, Prepare, Commit).

#### `network`: P2P Communication and Structure
-   **`committee/`**: Abstractions for node committees.
    -   `committee.h`/`.cpp`: A base class for committee structures.
    -   `final_committee.h`/`.cpp`: Manages the final committee that assembles `MacroBlock`s.
-   **`shard.h`/`.cpp`**: Manages a shard, including its transaction pool and PBFT process.
-   **`cross_shard.h`/`.cpp`**: Logic for handling cross-shard communication (if any).
-   **`mpi_wrapper.h`/`.cpp`**: Provides a higher-level wrapper around MPI communication primitives.

#### `util`: Utility and Helper Components
-   **`config.h`/`.cpp`**: Parses command-line arguments and manages configuration.
-   **`crypto.h`/`.cpp`**: Provides cryptographic functions (hashing, signing, verification).
-   **`errors.h`/`.cpp`**: Defines custom error handling utilities.
-   **`logging.h`/`.cpp`**: A simple logging framework.
-   **`metrics.h`/`.cpp`**: Utilities for collecting performance metrics.
-   **`serialization.h`/`.cpp`**: Helper functions for serializing/deserializing data structures.
-   **`threadpool.h`/`.cpp`**: A thread pool for concurrent task execution.
-   **`timer.h`/`.cpp`**: A high-resolution timer for performance measurement.

### Root & Testing
-   **`main.cpp`**: The main entry point for the simulation. It initializes MPI, configures the simulation based on `Config`, partitions nodes into shards, and orchestrates the primary workflow.
-   **`tests/test_pbft.cpp`**: Test suite for the PBFT implementation.
-   **`tests/test_sharding.cpp`**: Test suite for the sharding and network partitioning logic.

---

## Key Build & Execution Files

-   **`Makefile`**: Compiles all source code using `mpic++` and creates executables in `/build`.
-   **`scripts/run_experiment.sh`**: Automates running simulations on the cluster with varying parameters (e.g., number of shards, nodes).
-   **`scripts/plot_results.py`**: Parses experiment data from `/results` and generates performance graphs.
-   **`docs/PERFORMANCE.md`**: The final report for performance analysis and results.
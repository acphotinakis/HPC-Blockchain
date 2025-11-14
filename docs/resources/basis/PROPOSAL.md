# Project 7 Proposal: Parallelizing Blockchain Computations via Sharding

**Course:** CSCI 654 — Foundations of Parallel Computing  
**Professor:** Dr. Mohan Kumar  
**Team:** Andrew Photinakis, Caleb Talbott  
**Date:** November 11, 2025  

---

## 1. Problem Statement

Traditional blockchain architectures are linear, processing transactions sequentially. This design creates a significant scalability bottleneck, resulting in low transaction throughput and high latency, which limits their application for large-scale use. As outlined in the course materials (Blockchain.pdf), the core problem is one of scale.

Our project directly addresses this problem. Following the example prompt provided via email, our goal is to implement a parallel blockchain algorithm and "demonstrate how we can improve performance (1) in terms of time and (2) arriving at a consensus."

---

## 2. Proposed Solution: Sharding with PBFT

To achieve this parallelization, we will implement the Sharding method as described in the Blockchain.pdf slides (Page 10). This architecture allows for parallel processing by partitioning the network's workload.

Our model will be hierarchical and structured as follows:

- **Committees (Shards):** The network of nodes will be partitioned into several "committees" (shards).  

- **Transaction Partitioning:** A central pool of incoming transactions will be divided among these committees. Each committee is responsible only for processing its own subset of transactions (its "shard").  

- **Intra-Committee Consensus (PBFT):** As specified in the professor's email, we will implement the Practical Byzantine Fault Tolerance (PBFT) algorithm within each committee. This will allow the nodes in a single shard to agree on their own block of transactions.  

- **Parallel Block Creation:** Because each committee processes its transactions independently, multiple transaction blocks can be created and validated in parallel.  

- **Final Committee:** Upon reaching consensus, each committee will send its validated block (or block header) to a "final committee." This final committee is responsible for verifying the committee signatures and assembling the final, ordered blockchain.  


### Conceptual Flow

```
Transactions → Shard Partitioning → Parallel PBFT Consensus → Final Committee Aggregation → Global Chain
```

This sharding approach directly tackles the project goals by enabling multiple committees to arrive at a consensus in parallel, which should significantly increase transaction throughput.

---

## 3. Implementation Plan & Methodology

### 3.1. Language and Paradigm

**Language:** C++ (for performance and low-level control over process communication).  
**Parallel Paradigm:** Message Passing Interface (MPI). This paradigm is a strong fit for simulating a distributed, message-passing-based network of nodes required for consensus algorithms like PBFT.

---

### 3.2. Hardware

We will utilize the CS department's parallel systems, which are ideal for this MPI-based simulation:

**Development/Head Node:** tardis  
- 1 x 4-core Xeon E3-1220 v5  
- 64GB RAM  
- Debian 12, OpenMPI  

**Primary Compute Cluster:** kraken  
- Debian 12, OpenMPI  
- **CPU:** 2 x 22-core Xeon CPU E5-2696 v4 @ 2.20GHz (44 cores total)  
- **Memory:** 128GB  

**Notes:** This large core count will allow us to simulate a large-scale network by running many MPI processes, with each process representing a single node in the blockchain network.

---

### 3.3. Simulation Design

We will create a simulation of a high-volume transaction network. A main "client" process (Rank 0) will generate a large set of simulated transactions and distribute them to the appropriate shards.

To prove the effectiveness of our parallel design, we will compare two models:

**Baseline (Serial) Model:**  
A single-committee implementation. All MPI processes will belong to one consensus group and must sequentially validate the entire transaction pool using PBFT. This simulates a traditional, linear blockchain.

**Parallel (Sharded) Model:**  
The full hierarchical implementation. We will partition the MPI processes into N smaller communicators (shards) plus a "final committee" communicator. Each shard will only receive 1/N of the transactions and run PBFT in parallel.

---

## 4. Performance Measurement

Our methodology is to run both models under an identical transaction load and measure the performance gains as we scale the number of shards.

### 4.1. Key Metrics

| Metric | Description | Expected Trend |
|--------|--------------|----------------|
| **Throughput (TPS)** | Total transactions finalized in the global chain per second. | Should scale (near) linearly with the number of shards. |
| **Latency (Time)** | Average time from transaction submission to its final confirmation. | Should decrease as shard count increases. |
| **Speedup** | Time_Serial / Time_Parallel(N_Shards) | Should be greater than 1 and increase with N. |

---

### 4.2. Evaluation Process

1. Run the Baseline Model to establish a performance baseline (Time_Serial).  
2. Run the Parallel Model with N shards (e.g., N = 2, 4, 8, 16).  
3. Graph Throughput vs. N and Latency vs. N.  
4. Calculate and graph Speedup and Efficiency.

---

## 5. Project Timeline

| Milestone | Deliverable | Target Date |
|------------|--------------|-------------|
| **Project Review** | Serial PBFT baseline implemented; initial 2-shard model complete; preliminary performance data gathered. | 11/20/2025 |
| **Final Report Due** | Full experiments with multiple shard configurations (2–16); performance graphs and final report completed. | 12/05/2025 |
| **Final Demos** | Live simulation demo on kraken and final presentation showcasing performance improvements. | 12/11/2025 |

---

## 6. Expected Outcomes & Technical Implementation

**Primary Outcome:**  
A clear, data-driven demonstration that sharding (a form of data parallelism) significantly improves the throughput and latency of a PBFT-based consensus system.

**Scalability Analysis:**  
We will provide graphs and analysis showing the scalability of our solution, aiming to demonstrate near-linear speedup as we increase the number of shards.

**MPI Design:**  
- We will leverage the 44 cores of kraken to run numerous MPI processes, each simulating a distinct network node.  
- Each shard ("committee") will be implemented as a separate `MPI_Comm` (communicator).  
- Intra-shard consensus (PBFT) will be achieved using point-to-point (`MPI_Send`, `MPI_Recv`) and collective (`MPI_Bcast`, `MPI_Allgather`) operations within each communicator.  
- The "final committee" will use `MPI_Gather` or `MPI_Recv` to collect validated blocks from the leader of each shard.

---

## 7. References

- "A Survey of Blockchain Consensus Protocols." (20XX, Author(s) Unknown).
- "An efficient sharding consensus algorithm for consortium chains." (20XX, Author(s) Unknown).
- "Analyzing fault aware collective performance in a process fault tolerant MPI." (20XX, Author(s) Unknown).
- Castro, M., & Liskov, B. (1999). *"Practical Byzantine Fault Tolerance"*. Proceedings of the Third Symposium on Operating Systems Design and Implementation.
- Kumar, M. (2025). *"Parallelism and Blockchain"* (Blockchain.pdf). CSCI 654 Lecture Slides.
- Luu, L., et al. (2016). *"A Secure Sharding Protocol for Open Blockchains (Elastico)"*. Proceedings of the 2016 ACM SIGSAC Conference on Computer and Communications Security.
- "On Sharding Permissioned Blockchains." (20XX, Author(s) Unknown).
- "Reaching Consensus in the Byzantine Empire - A Comprehensive Survey." (20XX, Author(s) Unknown).
- "Survey of Sharding in Blockchains." (20XX, Author(s) Unknown).

---

## 8. Summary

This project connects the core principles of parallel computing (data partitioning, parallel processing, communication overhead) directly to a highly relevant, modern application: blockchain scalability. By implementing a PBFT-based sharding simulation in C++ and MPI on the kraken cluster, we will provide a practical, measurable, and scalable analysis of how parallel computation fundamentals can solve industry-scale problems.

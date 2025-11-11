# 🧩 CSCI 654: Project 7 Reference Paper Criteria

**Project:** *Parallelizing Blockchain Computations via Sharding (using PBFT and MPI)*
**Goal:** Build a “gold set” of reference papers to support the project’s implementation, methodology, and final report.
A paper is considered **high value** if it contributes to one or more of the research dimensions below.

---

## 🧠 Category 1: Foundational PBFT

Papers explaining the **core consensus mechanism** that will operate within each shard.

### 🔹 Criterion 1.1 — *The “What”*

* **Goal:** Understand the original PBFT protocol design.
* **Target Paper:** *Castro & Liskov (1999)* — *“Practical Byzantine Fault Tolerance.”*
* **Search Keywords:**

  * “PBFT consensus algorithm step-by-step”
  * “Pre-prepare prepare commit phases PBFT”
  * “Byzantine fault tolerance explanation”

### 🔹 Criterion 1.2 — *The “Why”*

* **Goal:** Understand PBFT’s scalability and communication bottlenecks.
* **Search Keywords:**

  * “PBFT message complexity O(N²)”
  * “PBFT scalability limitations”
  * “Why PBFT doesn’t scale”
  * “PBFT communication overhead analysis”

### 🔹 Criterion 1.3 — *The “How”*

* **Goal:** Learn about simplified or optimized PBFT variants.
* **Search Keywords:**

  * “Optimized PBFT implementations”
  * “Aggregate signatures PBFT”
  * “HotStuff consensus optimization”
  * “Simplified BFT protocols”

---

## 🧩 Category 2: Sharding Architectures

Papers defining the **parallel structure** of sharded blockchains and transaction partitioning.

### 🔹 Criterion 2.1 — *Foundational Sharding*

* **Goal:** Understand the first proposals for sharded blockchains.
* **Key References:** *Elastico (Luu et al., 2016)*, *Zilliqa (2019)*.
* **Search Keywords:**

  * “Elastico blockchain sharding”
  * “Zilliqa sharded blockchain architecture”
  * “Transaction partitioning blockchain”

### 🔹 Criterion 2.2 — *Hierarchical Models*

* **Goal:** Explore multi-layer sharding (committee + final committee).
* **Search Keywords:**

  * “Hierarchical sharding blockchain”
  * “Final committee consensus blockchain”
  * “Beacon chain sharding architecture (e.g., Ethereum 2.0)”

### 🔹 Criterion 2.3 — *Workload Partitioning*

* **Goal:** Learn how transactions are divided across shards.
* **Search Keywords:**

  * “Dynamic transaction partitioning blockchain”
  * “Workload balance sharded blockchain”
  * “Cross-shard transaction allocation”

---

## ⚙️ Category 3: The Intersection (PBFT + Sharding)

This is the **core category** connecting both ideas — how PBFT operates within a sharded network.

### 🔹 Criterion 3.1 — *Intra-Shard Consensus*

* **Goal:** Identify systems that use PBFT (or variants) inside each shard.
* **Search Keywords:**

  * “PBFT inside shard blockchain”
  * “BFT-based sharding”
  * “Committee-based consensus blockchain”
  * “Shard-level Byzantine fault tolerance”

### 🔹 Criterion 3.2 — *Comparative Analysis*

* **Goal:** Compare consensus mechanisms for sharded blockchains.
* **Search Keywords:**

  * “PBFT vs PoS in sharded blockchain”
  * “Consensus comparison for sharded networks”
  * “Why PBFT for permissioned blockchains”
  * “Consensus performance in distributed shards”

---

## 💻 Category 4: MPI-Based Simulation

Focus on **how to implement** distributed consensus and sharding using **MPI (Message Passing Interface)**.

### 🔹 Criterion 4.1 — *MPI for Consensus Simulation*

* **Goal:** Learn from MPI-based implementations of distributed algorithms.
* **Search Keywords:**

  * “MPI simulation of consensus algorithms”
  * “Implementing Raft/Paxos/BFT using MPI”
  * “MPI distributed systems simulation research paper”

### 🔹 Criterion 4.2 — *MPI Communicators for Shards*

* **Goal:** Understand how to structure “committees” using MPI communicators.
* **Search Keywords:**

  * “MPI_Comm group examples”
  * “MPI subcommunicators for distributed clusters”
  * “MPI communicator group creation tutorial”

### 🔹 Criterion 4.3 — *MPI Performance Optimization*

* **Goal:** Benchmark communication cost of MPI operations used in PBFT.
* **Search Keywords:**

  * “MPI_Bcast vs MPI_Send performance”
  * “MPI all-to-all communication performance”
  * “Scalability of MPI collective operations”
  * “MPI message latency benchmarking”

---

## 📊 Category 5: Performance Benchmarking

For validating **methodology and evaluation metrics** in your report.

### 🔹 Criterion 5.1 — *Standard Blockchain Metrics*

* **Goal:** Confirm that TPS, latency, and speedup are standard evaluation metrics.
* **Search Keywords:**

  * “Blockchain performance benchmarking”
  * “Transactions per second blockchain comparison”
  * “Latency analysis blockchain consensus”
  * “Parallel system speedup efficiency metrics”

### 🔹 Criterion 5.2 — *Scalability Graphs and Methodology*

* **Goal:** Find papers that visualize scalability trends.
* **Search Keywords:**

  * “Blockchain scalability throughput vs shards”
  * “PBFT scalability experiment results”
  * “Blockchain performance graph latency vs nodes”
  * “Sharded blockchain throughput linear scaling”

---

## 📚 Suggested Search Databases

Use the following sources for consistent, academic-quality results:

* **IEEE Xplore** — for PBFT, MPI, and parallel computing research.
* **ACM Digital Library** — for distributed systems and blockchain scalability papers.
* **arXiv.org (Computer Science > Distributed, Parallel, and Blockchain Systems)** — for recent preprints.
* **Google Scholar** — for quick cross-database keyword exploration.
* **SpringerLink / ScienceDirect** — for high-performance computing and MPI optimization studies.

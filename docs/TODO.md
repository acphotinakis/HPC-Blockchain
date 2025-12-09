# TODO.md

This checklist tracks all remaining work needed to complete missing or partially implemented features identified in the system analysis.

---

## 1. Transaction Nonce (Replay Protection)
**Status:** Partially Implemented  
**Goal:** Prevent replay attacks and enforce transaction ordering per account.

### ✅ Data Model Changes
- [ ] Add `uint64_t nonce` field to `Transaction` class  
  - Files:
    - `include/sbmpi/core/state/transaction.h`
    - `src/core/state/transaction.cpp`

### ✅ Transaction Hashing & Serialization
- [ ] Include `nonce` in `Transaction::constructHash()` input
- [ ] Update `serialize()` and `deserialize()` to read/write nonce
- [ ] Update `toJSON()` and `fromJSON()` to include nonce

### ✅ State Management
- [ ] Add nonce tracking to `State`
  - Add: `std::unordered_map<std::string, uint64_t> nonces`
  - Files:
    - `include/sbmpi/core/state/state.h`
    - `src/core/state/state.cpp`
- [ ] Initialize nonce entry to `0` for new addresses

### ✅ Transaction Verification Logic
- [ ] In `State::apply(const Transaction& tx)`:
  - Fetch expected nonce for `tx.from`
  - Reject transaction if `tx.nonce != expectedNonce`
- [ ] Increment sender nonce on successful transaction application

### ✅ Acceptance Criteria
- [ ] Replayed transactions are rejected
- [ ] Out-of-order nonces are rejected
- [ ] Valid sequential transactions succeed

---

## 2. Backup (Replica) Verification in PBFT
**Status:** Not Implemented  
**Goal:** Ensure non-leader replicas verify block contents before voting.

### ✅ Replica-Side Verification
- [ ] Locate replica execution path in `PBFT::run`
  - File:
    - `src/consensus/pbft.cpp`
- [ ] After `block.deserialize(blockData)`:
  - Iterate through `block.transactions`
  - Call `tx.verify()` for each transaction

### ✅ Conditional PREPARE Logic
- [ ] If *any* transaction fails verification:
  - Abort PREPARE phase for that block
  - (Optional) Trigger view change
- [ ] Only call `prepare(proposedBlockHash)` if all transactions pass

### ✅ Acceptance Criteria
- [ ] Replicas do not PREPARE invalid blocks
- [ ] Malicious leader cannot force bad transactions through consensus

---

## 3. Parallel Signature Verification
**Status:** Not Implemented  
**Goal:** Parallelize cryptographic verification using OpenMP.

### ✅ Build System
- [ ] Add OpenMP support to build system
  - Add `-fopenmp` to:
    - `CMakeLists.txt` and/or `Makefile`

### ✅ Shard Ingestion Phase
- [ ] Refactor ingestion logic in `Shard::runConsensus`
  - File:
    - `src/network/shard.cpp`
- [ ] Deserialize all incoming transactions into a vector first
- [ ] Parallelize verification loop using:
  ```cpp
  #pragma omp parallel for
````

* [ ] Ensure no shared mutable state during verification

### ✅ PBFT Replica Verification

* [ ] Apply same OpenMP strategy in PBFT replica verification loop

  * File:

    * `src/consensus/pbft.cpp`

### ✅ Acceptance Criteria

* [ ] All transaction verification paths remain thread-safe
* [ ] Parallel verification produces identical results to serial mode
* [ ] Performance improves with increasing transaction count

---

## 4. Fault Injection (Poisoned Transactions)

**Status:** Not Implemented
**Goal:** Prove system correctness by injecting intentionally invalid transactions.

### ✅ Configuration Support

* [ ] Add `double faultProbability` to `Config` struct

  * Files:

    * `include/sbmpi/util/config.h`
    * `src/util/config.cpp`
* [ ] Add command-line flag:

  * `--faults <probability>`

### ✅ Generator Enhancements

* [ ] Update `generateMockTransactions` to accept `faultProbability`

  * File:

    * `src/util/generator.cpp`
* [ ] Randomly select transactions to be faulty based on probability

### ✅ Fault Types

* [ ] **Type A — Integrity Failure**

  * Validly sign transaction
  * Mutate fields *after signing* (e.g., amount, receiver)
* [ ] **Type B — Authentication Failure**

  * Sign transaction with incorrect private key

### ✅ Fault Tracking & Logging

* [ ] Log or tag faulty transaction IDs during generation
* [ ] Verify same transactions are rejected during verification/consensus

### ✅ Acceptance Criteria

* [ ] Faulty transactions are consistently rejected
* [ ] System remains stable when faults are injected
* [ ] Logs clearly demonstrate fault detection

---

## ✅ Final Validation Checklist

* [ ] All unit tests pass
* [ ] Consensus does not finalize invalid blocks
* [ ] Replay attacks are impossible
* [ ] Parallel verification does not break determinism
* [ ] Fault injection proves correctness under adversarial conditions


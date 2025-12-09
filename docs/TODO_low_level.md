### 1\. Transaction Nonce (Replay Protection)

**Goal:** Ensure every transaction is unique and processed in order by the sender.

#### A. Modify `Transaction` Header

**File:** `include/sbmpi/core/state/transaction.h`

```cpp
class Transaction {
 public:
  // ... existing fields ...
  uint64_t nonce; // <--- ADD THIS

  // Update constructor signature
  Transaction(const std::string& from, const std::string& to, double amount, uint64_t nonce);
  
  // ... rest of class ...
};
```

#### B. Modify `Transaction` Implementation

**File:** `src/core/state/transaction.cpp`

1.  **Update Constructor:**

    ```cpp
    Transaction::Transaction(const std::string& from, const std::string& to,
                             double amount, uint64_t nonce)
        : from(from), to(to), amount(amount), nonce(nonce) // <--- Initialize nonce
    {
      time = std::chrono::system_clock::now().time_since_epoch().count();
      // ... rest of constructor ...
    }
    ```

2.  **Update Hashing (Crucial for Security):**

    ```cpp
    std::vector<unsigned char> Transaction::constructHash() const {
      std::stringstream ss;
      // Include nonce in the hash so changing it invalidates signature
      ss << from << "|" << to << "|" << amount << "|" << nonce << "|" << time; 
      // ... rest of hashing logic ...
    }
    ```

3.  **Update Serialization/Deserialization:**

    ```cpp
    std::vector<char> Transaction::serialize() const {
      std::vector<char> buffer;
      // ... pack existing fields ...
      util::pack(nonce, buffer); // <--- Add this (ensure util::pack overload exists for uint64_t)
      return buffer;
    }

    void Transaction::deserialize(const std::vector<char>& data) {
      int offset = 0;
      // ... unpack existing fields ...
      nonce = util::unpack_uint64(data, offset); // <--- Add this
    }
    ```

4.  **Update JSON Methods:**
    Add `{"nonce", nonce}` to `toJSON` and `nonce = json["nonce"].get<uint64_t>()` to `fromJSON`.

#### C. Modify State Logic

**File:** `include/sbmpi/core/state/state.h`
Add `std::unordered_map<std::string, uint64_t> accountNonces;` to the class.

**File:** `src/core/state/state.cpp`
Update `apply` to enforce order:

```cpp
bool State::apply(const Transaction& tx) {
  // 1. Signature Check
  if (!tx.verify()) return false;

  // 2. Nonce Check
  if (accountNonces[tx.from] != tx.nonce) {
    util::Logger::getLogger().error("Nonce mismatch! Expected: " + 
        std::to_string(accountNonces[tx.from]) + ", Got: " + std::to_string(tx.nonce));
    return false;
  }

  // 3. Balance Check (existing logic)
  // ...

  // 4. Update State
  // ... update balances ...
  accountNonces[tx.from]++; // <--- Increment nonce on success
  return true;
}
```

-----

### 2\. Backup (Replica) Verification

**Goal:** Replicas must validate block content before voting.

**File:** `src/consensus/pbft.cpp`

Modify `PBFT::run` inside the `else` block (where replicas receive data):

```cpp
// ... inside PBFT::run ...
} else {
  // Replicas receive the block content
  std::vector<char> blockData;
  network::bcast(blockData, leaderRank, communicator);
  messagesExchanged++;
  block.deserialize(blockData);
  
  // --- NEW: REPLICA VERIFICATION LOGIC ---
  bool isBlockValid = true;
  
  // We will parallelize this loop in the next section, but here is the logic:
  for (const auto& tx : block.transactions) {
      if (!tx.verify()) {
          util::Logger::getLogger().error("Replica detected INVALID transaction: " + tx.id);
          isBlockValid = false;
          break; // Optimization: fail fast
      }
  }

  if (!isBlockValid) {
      util::Logger::getLogger().error("Block verification failed. Replica will NOT vote.");
      // Return early or enter a view-change state. 
      // For this simulation, returning empty/error is sufficient to stop consensus.
      return {core::blocks::MicroBlock(), messagesExchanged}; 
  }
  // ----------------------------------------

  util::Logger::getLogger().debug("PBFT Replica: Verified block proposal.");
}
```

-----

### 3\. Parallel Signature Verification

**Goal:** Use OpenMP to verify transaction signatures concurrently.

#### A. Build System Update

**File:** `CMakeLists.txt`
Ensure OpenMP is linked:

```cmake
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(sbmpi PUBLIC OpenMP::OpenMP_CXX)
endif()
```

#### B. Parallelize Ingestion (Shard Leader)

**File:** `src/network/shard.cpp`
In `runConsensus`, refactor the ingestion loop:

```cpp
// 1. Deserialize ALL transactions first (must be serial)
std::vector<core::state::Transaction> tempTxList;
tempTxList.reserve(numTx);
for (int i = 0; i < numTx; ++i) {
    // ... unpacking logic ...
    core::state::Transaction tx;
    tx.deserialize(txData);
    tempTxList.push_back(tx);
    offset += txSize;
}

// 2. Parallel Verify
std::vector<int> validity(numTx, 0); // 1 = valid, 0 = invalid

#pragma omp parallel for
for (int i = 0; i < numTx; ++i) {
    if (tempTxList[i].verify()) {
        validity[i] = 1;
    }
}

// 3. Serial Mempool Update
for (int i = 0; i < numTx; ++i) {
    if (validity[i]) {
        mempool.push_back(tempTxList[i]);
    } else {
        util::Logger::getLogger().error("Invalid Tx discarded: " + tempTxList[i].id);
    }
}
```

#### C. Parallelize Replica Verification

**File:** `src/consensus/pbft.cpp`
Apply similar logic to the verification loop added in Step 2:

```cpp
std::atomic<bool> allValid(true);

#pragma omp parallel for
for (size_t i = 0; i < block.transactions.size(); ++i) {
    if (!allValid) continue; // Skip if already failed
    if (!block.transactions[i].verify()) {
        allValid = false;
    }
}

if (!allValid) {
    // Abort...
}
```

-----

### 4\. Fault Injection

**Goal:** Introduce malicious transactions to test the verification logic.

#### A. Update Configuration

**File:** `include/sbmpi/util/config.h`
Add `double faultProbability = 0.0;` to the `Config` struct.

#### B. Generator Logic

**File:** `src/util/generator.cpp`
Update `generateMockTransactions` to accept `faultProbability`.

```cpp
// In generateMockTransactions loop:

// 1. Create and Sign VALID transaction first
sbmpi::core::state::Transaction tx(senderAddress, receiverAddress, amount, currentNonce);
tx.sign(senderWallet.privateKeyRaw);

// 2. Roll for Fault
std::uniform_real_distribution<> faultDist(0.0, 1.0);
if (faultDist(gen) < faultProbability) {
    // Type A: Modify Content (breaks Integrity)
    // Changing amount AFTER signing invalidates the hash signature
    tx.amount += 1000000.0; 
    
    // OR Type B: Bad Key (breaks Authenticity)
    // Re-sign with a random key (simulating impersonation)
    // tx.sign(random_key); 
    
    std::cout << "[GENERATOR] Injected FAULT into Tx: " << tx.id << std::endl;
}

transactions.push_back(tx);
```

#### C. Main Integration

**File:** `src/main.cpp`

  * Parse `--faults` arg in `Config::parse`.
  * Pass `config.faultProbability` to `generateMockTransactions`.
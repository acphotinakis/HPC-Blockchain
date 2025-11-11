# Research Topics & Search Queries for “Parallelism and Blockchain”
--------------------------------------------------------------------------------------------------------


# Blockchain Fundamentals

## How Blockchain Works Step by Step

* Is fundamentally a decentralized, distributed, and immutable ledger
* Records transactions across many computers so that the record cannot be altered retroactively without the alteration of all subsequent blocks and the concensus of a network
* Step by Step
    - Transaction Initiation
        - User initiates a transaction (e.g. sending crypto, or transferring a digital asset)
        - Transaction is packaged with sender's public key, recipients public key, and data/amount
    - Transaction Broadcast
        - Initiated transaction is broadcasted to P2P network and sits in a pool of unconfirmed transactions (the mempool)
    - Validation by Nodes (Miners/Validators)
        - Nodes (like miners in Proof-of-Work or validators in Proof-of-Stake) on network receive transaction and verify validity using cryptographic methods
        - Includes checking sender's digital signature and ensuring they have necessary funds or authority
    - Block Creation
        - Validated transactions are collected into a new block
        - Ndoe adds a header to this block, which includes a timestamp, cryptographic hash of previous block, and nonce (for PoW chains) or signature (for PoS chains)
    - Consensus and Mining
        - Block-creating node must now solve complex computational problem (e.g. find a hash below a target for Proof-of-Work) or meet certain staking requirements (for Proof-of-Stake) to validate block and reach consensus with network
        - Process ensures all participents agree on state of ledger
    - Block Added
        - Once node successfully validates the block (i.e. "mines" it), they broadcast it to rest of network
        - Other nodes verify block's validity
            - If accepted, they add this new block to copy of their blockchain, and the validated transactions are now permenently recorded
    - Execution
        - Transaction is considered complete, and record of distributed ledger is updated

### What is a Distributed Ledger System (DLS)?
* Is a database that is shared, replicated, and synchronized across multiple participants in a computer network
* Unlike centralized database, DLS doesn't rely on single authority to maintain a record, as data is spread across all participants
    - Makes DLS more transparent, secure, and highly resistant to tampering
* New transactions are verified and added through a concensus mechanism, where all participants agree on validity of new data before added to their identical copies of the ledger
- Components
    - Ledger
        - Refers to histofical record of transactions
    - Distributed
        - Instead of central authority (like a government or bank) holding only the master copy, every participant/node has this own identical copy of ledger
    - System
        - Refers to network protocols and consensus mechanisms that govern how ledger is udpated and maintained
- Key Benefit of a DLS 
    - Redundancy and security
        - Even if one or more nodes fail, network continues to operate and data remains available and intact on remaining nodes

### How Blocks are Linked Using Cryptographic Hash Pointers

- Fundamental data structure that makes a blockchain a "chain" is use of cryptographic hash pointers
    - Hash Pointer
        - Unlike standard pointer, which only contains address of next data location
        - Contains both address of previous block and cryptographic hash of entire content of previous block
        - $$\text{Block}_n = \left( \text{Data}_n, \text{Hash Pointer to } \text{Block}_{n-1} \right)$$
    - Linkage
        - When new block $$\text{Block}_n$$ is created, its header includes hash of previous block $$\text{Block}_{n-1}$$
        - Effectively links $$\text{Block}_n$$ to $$\text{Block}_{n-1}$$
    - Security Implication
        - B/c hash of a block is determined by its complete data (transactions, timestamp, previous hash), any attempt to tamper with data inside an old block ($$\text{Block}_{n-1}$$) will immediately change its unique cryptographic has
            - This change would invalidate the hash pointer stored in next block ($$\text{Block}_{n}$$)
            - Since network validates these hashes, entire chain would be broken from that point forward
                - Makes attempt obvious and easily rejected by decentralized network


### Blockchain Immutability Explained





### “Blockchain data structure linked list hash chain”

### “Genesis block meaning in blockchain”



## What is a distributed ledger system? - Distributed Ledger Syustem (DLS)

### What is it? 

* Is a database that is shared, replicated, and synchronized across multiple participants in a computer network
* Unlike centralized database, DLS doesn't rely on single authority to maintain a record, as data is spread across all participants
    ** Makes DLS more transparent, secure, and highly resistant to tampering
* New transactions are verified and added through a concensus mechanism, where all participants agree on validity of new data before added to their identical copies of the ledger

### How it works?

* Shared Copies: Every participent in network has identical copy of entire ledger
* Peer-to-Peer Network: Pariticpants are connected in a P2P network, eliminating need for central intermediary
* Concensus: To add new transactions, network participants must agree on validity of transaction through concensus mechanism. Once concensus reached, transaction is added to all copies of ledger simulataneously
* Cryptography: Transactions are secured using cryptography, making them permanent and difficult to alter or delete once recorded

### Key Benefits

* Transparency: All particiapnts can see the same version of the ledger, which increases transparency
* Security: Distributed nature and cryptographic security make very difficult for single attacker to tamper with data. No single point of failture, and a change to one copy would be immediately noticable to other participants
* Efficiency: By removing intermediares, transactions can be processed more quickly and at a lower cost
* Reliability: B/c data is replicated across many computers, system remains functional even if some participants' systems go offline

### Examples of use

* Cryptocurrencies: Bitcoin and Ethereum 
* Supply change management: Tracking goods from production to delivery
* Voting systems: Creating a secure and transparent record of votes
* Digital identity: Providing a secure and verifiable digital identity for individuals
* Smart contracts: Automated contracts with terms of agreement written directly into code
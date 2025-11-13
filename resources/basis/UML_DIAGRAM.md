# UML Class Diagram

This document contains a UML class diagram representing the planned software architecture for the Parallelizing Blockchain Computations project. The diagram is based on the analysis of the `CONCEPT_MAP.md`, `IMPLEMENTATION_PLAN.md`, and `STRUCTURE.md` documents.

```mermaid
classDiagram
    direction LR

    class Main {
        <<Orchestrator>>
        +main(argc, argv) void
        +setupShards(int num_nodes, int num_shards) void
        +distributeTransactions(List~Transaction~) void
    }

    class Node {
        <<Data Container>>
        -int global_rank
        -int shard_rank
        -int shard_id
        -Role role
    }

    class Transaction {
        <<Data Structure>>
        -string id
        -string sender
        -string receiver
        -double amount
        +serialize() byte[]
        +deserialize(byte[]) Transaction
    }

    class Block {
        <<Data Structure>>
        -int height
        -string previous_hash
        -long timestamp
        -List~Transaction~ transactions
    }

    class Blockchain {
        <<Ledger>>
        -List~Block~ chain
        +addBlock(Block) void
        +isValid() bool
    }

    class Shard {
        <<Committee Manager>>
        -MPI_Comm shard_comm
        -List~Node~ nodes
        -List~Transaction~ transaction_pool
        -PBFT pbft_instance
        +runConsensus() Block
    }

    class PBFT {
        <<Consensus Algorithm>>
        -MPI_Comm communicator
        -PBFT_State state
        -List~Message~ message_log
        +executeConsensus(List~Transaction~) Block
        -broadcastPrePrepare() void
        -broadcastPrepare() void
        -broadcastCommit() void
    }

    Main ..> Node : Creates/Manages
    Main ..> Shard : Creates/Manages
    Main ..> Transaction : Generates
    Main ..> Blockchain : Manages Final Chain

    Shard "1" o-- "many" Node : Contains
    Shard "1" *-- "1" PBFT : Owns/Uses
    Shard "1" o-- "many" Transaction : Processes

    PBFT ..> Transaction : Operates on
    PBFT ..> Block : Creates

    Blockchain "1" *-- "many" Block : Composed of
    Block "1" *-- "many" Transaction : Composed of
```

## Diagram Legend

-   **`Main`**: The main driver of the simulation. It initializes MPI, partitions nodes into shards, generates and distributes transactions, and orchestrates the overall workflow.
-   **`Node`**: A simple data class representing a single MPI process, holding its rank and role within the system.
-   **`Transaction`**: A data structure for a single transaction. It must be serializable to be sent over the network.
-   **`Block`**: A data structure containing a list of transactions and a header with metadata, forming one link in the blockchain.
-   **`Blockchain`**: Represents the final, global ledger, which is a collection of blocks aggregated from all shards.
-   **`Shard`**: Manages a committee of nodes. It holds the shard-specific MPI communicator, its transaction pool, and orchestrates the consensus process by using its `PBFT` instance.
-   **`PBFT`**: The core consensus engine. It runs within a shard's communicator to validate a set of transactions and produce a block.
-   **Relationships**:
    -   `..>` : Dependency (Uses)
    -   `o--` : Aggregation (Has-a)
    -   `*--` : Composition (Owns-a)

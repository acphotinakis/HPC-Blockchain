# UML Class Diagram (Updated)

This document contains an updated UML class diagram representing the implemented software architecture. This diagram reflects the more detailed and modular structure of the final codebase.

```mermaid
classDiagram
    direction LR

    class Main {
        <<Orchestrator>>
        +main(argc, argv) void
    }

    class Config {
        <<Utility>>
        +int num_shards
        +int num_transactions
        +parse(argc, argv) bool
    }

    class Node {
        <<Data Container>>
        -int global_rank
        -int shard_id
        -NodeRole role
    }

    class Transaction {
        <<Data Structure>>
        -string id
        +serialize() vector~char~
        +deserialize(vector~char~) void
    }

    class Block {
        <<Abstract>>
        #BlockHeader header
        #vector~Transaction~ transactions
        +getHash() string
        +serialize() vector~char~
        +deserialize(vector~char~) void
    }

    class MicroBlock {
        <<Concrete Block>>
        -int shard_id
    }

    class MacroBlock {
        <<Concrete Block>>
        -vector~string~ micro_block_hashes
    }

    class BlockHeader {
        <<Data Structure>>
        -string previous_hash
        -string merkle_root
        +hash() string
    }

    class Blockchain {
        <<Ledger>>
        -vector~unique_ptr~Block~~ chain
        +addBlock(unique_ptr~Block~) void
    }

    class Shard {
        <<Committee Manager>>
        -MPI_Comm shard_comm
        -vector~Transaction~ mempool
        -PBFT pbft_instance
        +runConsensus() MicroBlock
    }

    class FinalCommittee {
        <<Committee Manager>>
        -MPI_Comm final_comm
        +collectMicroBlocks() vector~MicroBlock~
        +assembleMacroBlock(vector~MicroBlock~) MacroBlock
    }

    class PBFT {
        <<Consensus Algorithm>>
        -MPI_Comm communicator
        +run(vector~Transaction~) MicroBlock
    }

    Main ..> Config : Uses
    Main ..> Node : Creates
    Main ..> Shard : Creates
    Main ..> FinalCommittee : Creates
    Main ..> Blockchain : Manages

    Shard "1" *-- "1" PBFT : Owns/Uses
    Shard "1" o-- "many" Transaction : Manages in Mempool
    Shard ..> MicroBlock : Creates

    FinalCommittee ..> MicroBlock : Collects
    FinalCommittee ..> MacroBlock : Creates

    Blockchain "1" *-- "many" Block : Composed of (Polymorphic)
    Block <|-- MicroBlock
    Block <|-- MacroBlock
    Block "1" *-- "1" BlockHeader : Contains
    MicroBlock "1" *-- "many" Transaction : Contains
    MacroBlock "1" o-- "many" MicroBlock : Aggregates Hashes Of

    PBFT ..> Transaction : Operates on
    PBFT ..> MicroBlock : Produces
```

## Diagram Legend (Updated)

-   **`Main`**: The simulation driver. Uses `Config` to set up the environment, then creates and manages `Shard`s and the `FinalCommittee`.
-   **`Config`**: Parses and holds simulation parameters.
-   **`Node`**: Holds a process's identity and role.
-   **`Transaction`**: A serializable data structure for a single transaction.
-   **`Block`**: Abstract base class for blocks.
-   **`MicroBlock`**: Concrete block produced by a `Shard`. Contains transactions.
-   **`MacroBlock`**: Concrete block produced by the `FinalCommittee`. Contains hashes of `MicroBlock`s.
-   **`BlockHeader`**: Metadata for a `Block`.
-   **`Blockchain`**: The global ledger, composed of a polymorphic list of `Block`s (primarily `MacroBlock`s).
-   **`Shard`**: Manages a shard committee, its `Mempool`, and runs a `PBFT` instance to produce a `MicroBlock`.
-   **`FinalCommittee`**: Manages the final committee, which collects `MicroBlock`s and assembles them into `MacroBlock`s.
-   **`PBFT`**: The consensus engine that validates transactions and creates a `MicroBlock`.
-   **Relationships**:
    -   `..>` : Dependency (Uses)
    -   `o--` : Aggregation (Has-a)
    -   `*--` : Composition (Owns-a)
    -   `<|--`: Inheritance
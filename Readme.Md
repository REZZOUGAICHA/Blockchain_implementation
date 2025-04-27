
# Simple Blockchain Implementation in C

## Overview
This code implements a basic blockchain system with Merkle trees in C. It creates a data structure to store transactions securely using cryptographic hashing while maintaining historical integrity through linked blocks.

## Blockchain Structure
The blockchain is a linked list of blocks, where each block contains a Merkle tree of its events.

### Global Architecture
```plaintext
Blockchain
├── genesis: Block* (first block)
├── last_block: Block* (last confirmed block)
├── block_count: int
└── current_mining_block: Block* (block being mined)
```

### Block Structure
```plaintext
Block
├── index: int (sequential ID)
├── timestamp: time_t
├── previous_hash: char[HASH_SIZE+1]
├── events: Event* (dynamic array)
│   ├── type: int
│   ├── data: char[256]
│   ├── timestamp: char[30]
│   └── hash: char[HASH_SIZE+1]
├── event_count: int
├── event_capacity: int
├── nonce: int
├── merkle_root: char[HASH_SIZE+1]
├── hash: char[HASH_SIZE+1]
└── next: Block* (link to next block)
```

### Merkle Tree Structure
```plaintext
MerkleNode
├── hash: char[HASH_SIZE+1]
├── left: MerkleNode*
└── right: MerkleNode*
```

## Data Flow Graph

```mermaid
graph TD
    Blockchain["Blockchain"]
    Block["Block"]
    Event["Event"]
    MerkleNode["MerkleNode"]

    Blockchain -->|contains| Block
    Block -->|contains| Event
    Block -->|uses for merkle_root| MerkleNode
    MerkleNode -->|left child| MerkleNodeLeft[Child Node]
    MerkleNode -->|right child| MerkleNodeRight[Child Node]

    Blockchain -->|functions| CreateBlockchain[+create_blockchain()]
    Blockchain -->|functions| AddEvent[+add_event_to_blockchain()]
    Blockchain -->|functions| ConfirmBlock[+confirm_block()]
    Blockchain -->|functions| PrintBlockchain[+print_blockchain()]
    Blockchain -->|functions| FreeBlockchain[+free_blockchain()]

    Block -->|functions| CreateBlock[+create_block()]
    Block -->|functions| AddEventBlock[+add_event_to_block()]
    Block -->|functions| CalcMerkle[+calculate_merkle_root()]
    Block -->|functions| CalcHash[+calculate_block_hash()]
    Block -->|functions| PrintBlock[+print_block()]

    MerkleNode -->|functions| CreateNode[+create_merkle_node()]
    MerkleNode -->|functions| BuildTree[+build_merkle_tree()]
    MerkleNode -->|functions| CalcParentHash[+calculate_parent_hash()]
```


## Merkle tree
```mermaid
graph TD
    Root[("Merkle Root
    (Stored in Block Header)")]
    
    Root --> Parent1["Parent Hash 1"]
    Root --> Parent2["Parent Hash 2"]
    
    Parent1 --> Leaf1["Event Hash 1"]
    Parent1 --> Leaf2["Event Hash 2"]
    
    Parent2 --> Leaf3["Event Hash 3"]
    Parent2 --> Leaf4["Event Hash 4"]
    
    %% For odd number of leaves
    Root -.->|If odd leaves| Parent3["Parent Hash 3 (Duplicate of last hash)"]
    Parent3 --> Leaf5["Event Hash 5"]
    Parent3 --> Leaf5Copy["Event Hash 5 (Copy)"]

    classDef node fill:#f9f,stroke:#333,stroke-width:2px;
    classDef leaf fill:#9f9,stroke:#333,stroke-width:2px;
    class Root,Parent1,Parent2,Parent3 node;
    class Leaf1,Leaf2,Leaf3,Leaf4,Leaf5,Leaf5Copy leaf;
```
---

## Key Features
- **Block Creation and Linking**: New blocks maintain the chain's integrity by including the previous block's hash.
- **Dynamic Event Management**: Blocks can store multiple events with automatic capacity adjustment.
- **Merkle Tree Integration**: Events in each block are secured using a Merkle tree structure.
- **Memory Management**: Proper allocation and deallocation to prevent memory leaks.

## Code Structure
- **Data Structures**: `Event`, `MerkleNode`, `Block`, and `Blockchain`
- **Hashing Functions**: Simple hash implementation (replaceable with cryptographic algorithms)
- **Merkle Tree Operations**: Creation, maintenance, and memory management
- **Blockchain Operations**: Block creation, event addition, and chain validation

## Usage Example
The `main` function demonstrates creating a blockchain, adding various transaction events, and displaying the resulting chain structure.




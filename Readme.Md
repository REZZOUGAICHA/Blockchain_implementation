# Simple Blockchain Implementation in C

## Overview
This code implements a basic blockchain system with Merkle trees in C. It creates a data structure to store transactions securely using cryptographic hashing while maintaining historical integrity through linked blocks.

## Blockchain Structure

### Global Architecture
Blockchain
├── genesis: Block* (premier bloc)
├── last_block: Block* (dernier bloc confirmé)
├── block_count: int
└── current_mining_block: Block* (bloc en cours de minage)


### Block Structure
Block
├── index: int (ID séquentiel)
├── timestamp: time_t
├── previous_hash: char[HASH_SIZE+1]
├── events: Event* (tableau dynamique)
│ ├── type: int
│ ├── data: char[256]
│ ├── timestamp: char[30]
│ └── hash: char[HASH_SIZE+1]
├── event_count: int
├── event_capacity: int
├── nonce: int
├── merkle_root: char[HASH_SIZE+1]
├── hash: char[HASH_SIZE+1]
└── next: Block* (lien vers bloc suivant)


### Merkle Tree Structure
MerkleNode
├── hash: char[HASH_SIZE+1]
├── left: MerkleNode*
└── right: MerkleNode*


## Data Flow Graph

```mermaid
graph TD
    Blockchain --> GenesisBlock
    Blockchain --> LastBlock
    Blockchain --> MiningBlock
    
    GenesisBlock -->|next| Block1
    Block1 -->|next| Block2
    Block2 -->|next| BlockN
    BlockN --> LastBlock
    
    MiningBlock -->|will become| NextBlock
    
    each Block --> Events
    each Block --> MerkleTree
    
    Events --> Event1
    Events --> Event2
    Events --> EventN
    
    MerkleTree -->|root| MerkleRoot
    MerkleTree --> Leaf1[Hash Event1]
    MerkleTree --> Leaf2[Hash Event2]
    MerkleTree --> LeafN[Hash EventN]
    
## Key Features
Block Creation and Linking: New blocks maintain the chain's integrity by including the previous block's hash

Dynamic Event Management: Blocks can store multiple events with automatic capacity adjustment

Merkle Tree Integration: Events in each block are secured using a Merkle tree structure

Memory Management: Proper allocation and deallocation to prevent memory leaks

Code Structure
Data Structures: Event, MerkleNode, Block, and Blockchain

Hashing Functions: Simple hash implementation (replaceable with cryptographic algorithms)

Merkle Tree Operations: Creation, maintenance, and memory management

Blockchain Operations: Block creation, event addition, and chain validation

Usage Example
The main function demonstrates creating a blockchain, adding various transaction events, and displaying the resulting chain structure.

Technical Notes
Uses a simplified hash function (for educational purposes only)

Implements proper memory management to prevent leaks

Includes both recursive and iterative approaches to tree traversal

Future Improvements
Replace the simple hash with cryptographic functions (SHA-256)

Add proof-of-work or consensus mechanisms

Implement transaction validation

Enhance security features


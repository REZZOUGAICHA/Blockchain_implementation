#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

/* 
 * BLOCKCHAIN CONFIGURATION
 */
#define HASH_SIZE 64                // Length of hash strings
#define MAX_EVENTS 100              // Maximum events per block( event is transaction or other event like smart contract execution ...)
#define MAX_NODES 10                // Maximum number of nodes in the network
#define DIFFICULTY 2                // Number of leading zeros required for Proof of Work
#define CONSENSUS_THRESHOLD 0.51    // 51% of nodes must agree for consensus

/* 
 * CORE DATA STRUCTURES
 */

// Event 
typedef struct {
    int type;                      // Transaction type (1 = financial transaction, 2- any other kind of events)
    char data[256];                // JSON-formatted transaction data
    char timestamp[30];            // When the transaction was created
    char hash[HASH_SIZE+1];        // Unique identifier for this transaction
    bool is_valid;                 // Validation status flag
} Event;

// Merkle Tree Node - Used to efficiently validate transaction integrity
typedef struct MerkleNode {
    char hash[HASH_SIZE+1];        // Hash value at this node
    struct MerkleNode* left;       // Left child (if any)
    struct MerkleNode* right;      // Right child (if any)
} MerkleNode;

// Block - A container for multiple events/transactions
typedef struct Block {
    int index;                     // Position in the blockchain (0 = genesis block)
    time_t timestamp;              // When the block was created
    char previous_hash[HASH_SIZE+1]; // Hash of the previous block (forms the chain)
    Event* events;                 // Array of events contained in this block
    int event_count;               // Number of events currently in the block
    int event_capacity;            // Maximum events this block can hold before resizing
    int nonce;                     // Number used once for Proof of Work
    char merkle_root[HASH_SIZE+1]; // Root hash of the Merkle tree of all events
    char hash[HASH_SIZE+1];        // Hash of this entire block, Prevents needing to recalculate the hash every time it's needed
    // also so that it Contains the result of the mining process (the valid hash that meets difficulty requirements)
    struct Block* next;            // Pointer to the next block in the chain
} Block;

// Blockchain 
typedef struct {
    Block* genesis;                // First block in the chain ("genesis" block)
    Block* last_block;             // Most recent confirmed block
    int block_count;               // Total number of blocks in the chain
    Block* current_mining_block;   // Block currently being assembled (not yet confirmed)
    pthread_mutex_t lock;          // Lock for thread-safe operations
} Blockchain;

// Node 
typedef struct {
    int id;                        // Unique identifier for this node
    Blockchain* chain;             // This node's copy of the blockchain
    bool is_mining;                // Whether this node is actively mining
    bool is_malicious;             // Whether this node attempts to tamper with data
    bool is_active;                // Whether this node is currently online
    pthread_t thread;              // Thread handling this node's operations
} Node;

/* 
 * GLOBAL VARIABLES 
 */

Node nodes[MAX_NODES];             // Array of all nodes in the network
int node_count = 0;                // Current number of active nodes
pthread_mutex_t nodes_lock = PTHREAD_MUTEX_INITIALIZER; // Lock for thread-safe node operations
bool shutdown_requested = false;   // Flag to signal system shutdown

/*
 * HASHING FUNCTIONS
 * Core cryptographic operations for data integrity
 */

// Simple hash function just to simulate my tp, hash function library didn't work for me 
void hash_data(const char* input, char* output) {
    unsigned long hash = 5381;
    while (*input) hash = ((hash << 5) + hash) + *input++;
    sprintf(output, "%016lx", hash);
    
    // Fill with zeros to reach HASH_SIZE
    int len = strlen(output);
    memset(output + len, '0', HASH_SIZE - len);
    output[HASH_SIZE] = '\0';
}

/*
 * EVENT OPERATIONS
 * Functions for handling individual transactions
 */

// Generate a unique hash for an event based on its contents
void hash_event(Event* event) {
    // Combine all event data to create a unique hash
    char buffer[512];
    sprintf(buffer, "%d%s%s", event->type, event->data, event->timestamp);
    hash_data(buffer, event->hash);
}

/*
 * MERKLE TREE OPERATIONS
 * Functions for creating and managing the Merkle tree data structure
 */

// Create a new node in the Merkle tree
MerkleNode* create_merkle_node(const char* hash) {
    MerkleNode* node = malloc(sizeof(MerkleNode));
    strcpy(node->hash, hash);
    node->left = node->right = NULL;
    return node;
}

// Free the memory used by a Merkle tree
void free_tree(MerkleNode* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

// Recursively build a Merkle tree from an array of hashes
MerkleNode* build_tree(char** hashes, int start, int end) {
    // Base cases: empty range or single hash
    if (start > end) return NULL;
    if (start == end) return create_merkle_node(hashes[start]);
    
    // Divide and conquer: build left and right subtrees
    int mid = (start + end) / 2;
    MerkleNode* left = build_tree(hashes, start, mid);
    MerkleNode* right = build_tree(hashes, mid + 1, end);
    
    // For odd number of nodes, duplicate the last one
    // This ensures every parent has exactly two children
    if (!right) right = create_merkle_node(left->hash);
    
    // Create parent node with combined hash of children
    MerkleNode* parent = create_merkle_node("");
    char combined[HASH_SIZE*2 + 1];
    sprintf(combined, "%s%s", left->hash, right->hash);
    hash_data(combined, parent->hash);
    
    parent->left = left;
    parent->right = right;
    return parent;
}

// Calculate the Merkle root hash for a block's events
void calculate_merkle_root(Block* block) {
    if (block->event_count == 0) {
        // Empty block gets all zeros for its Merkle root
        memset(block->merkle_root, '0', HASH_SIZE);
        block->merkle_root[HASH_SIZE] = '\0';
        return;
    }
    
    // Collect all event hashes
    char** hashes = malloc(block->event_count * sizeof(char*));
    for (int i = 0; i < block->event_count; i++) {
        hashes[i] = block->events[i].hash;
    }
    
    // Build the Merkle tree and get its root hash
    MerkleNode* root = build_tree(hashes, 0, block->event_count - 1);
    strcpy(block->merkle_root, root->hash);
    
    // Clean up
    free_tree(root);
    free(hashes);
}

/*
 * BLOCK OPERATIONS
 * Functions for creating and managing blocks
 */

// Generate a unique hash for a block based on its contents
void hash_block(Block* block) {
    char buffer[1024];
    sprintf(buffer, "%d%ld%s%s%d", 
            block->index, block->timestamp, 
            block->previous_hash, block->merkle_root, block->nonce);
    hash_data(buffer, block->hash);
}

// Create a new empty block with the given index and previous hash
Block* create_block(int index, const char* prev_hash) {
    Block* block = malloc(sizeof(Block));
    
    block->index = index;
    block->timestamp = time(NULL);  // Current time
    strcpy(block->previous_hash, prev_hash);
    
    // Initialize events array with initial capacity
    block->event_capacity = 10;  // Start with space for 10 events
    block->events = malloc(block->event_capacity * sizeof(Event));
    block->event_count = 0;
    block->nonce = 0;  // Will be determined during mining
    block->next = NULL;
    
    return block;
}

// Create a deep copy of a block - for nodes 
Block* clone_block(Block* source) {
    Block* block = malloc(sizeof(Block));
    
    // Copy all basic properties
    block->index = source->index;
    block->timestamp = source->timestamp;
    strcpy(block->previous_hash, source->previous_hash);
    strcpy(block->merkle_root, source->merkle_root);
    strcpy(block->hash, source->hash);
    block->nonce = source->nonce;
    block->next = NULL;  // we don't copy next pointer 
    //cus cloned blocks are meant to be temporary working copies, not full chain replicas
    
    // Clone events array
    block->event_capacity = source->event_capacity;
    block->event_count = source->event_count;
    block->events = malloc(block->event_capacity * sizeof(Event));
    memcpy(block->events, source->events, source->event_count * sizeof(Event));
    
    return block;
}

// Free the memory used by a block
void free_block(Block* block) {
    if (block) {
        free(block->events);
        free(block);
    }
}

// Check if a block's hash meets the difficulty requirement (Proof of Work)
bool is_valid_proof(Block* block, int difficulty) {
    // Hash must start with 'difficulty' number of zeros
    for (int i = 0; i < difficulty; i++) {
        if (block->hash[i] != '0') return false;
    }
    return true;
}

/* 
 * MINING OPERATIONS
 * The process of finding a valid nonce to create a valid block hash
 */

// Mine a block by finding a nonce that produces a valid hash
// Proof of Work algorithm :
bool mine_block(Block* block, int difficulty) {
    // Start with nonce = 0
    block->nonce = 0;
    calculate_merkle_root(block);
    
    // Create target string (e.g., "00ffff..." for difficulty=2)
    char target[HASH_SIZE+1];
    memset(target, '0', difficulty);  // Leading zeros
    memset(target + difficulty, 'f', HASH_SIZE - difficulty);  // Rest is 'f'
    target[HASH_SIZE] = '\0';
    
    // Keep trying different nonce values until we find a valid hash
    while (true) {
        hash_block(block);
        
        // Check if hash meets difficulty requirement
        if (strncmp(block->hash, target, difficulty) <= 0) {
            return true;  // Found a valid nonce!
        }
        
        block->nonce++;  // Try next nonce value
        
        // Mining simulation - introduce delay and early termination chance
        if (block->nonce % 10 == 0) {
            usleep(10000);  // 10ms pause to prevent CPU overload
            
            // 1% chance to simulate finding solution (speeds up simulation)
            if (rand() % 100 < 1) {
                hash_block(block);
                return true;
            }
            
            // Check if we need to stop mining
            if (shutdown_requested) return false;
        }
    }
}

/*
 * VALIDATION FUNCTIONS
 * Ensure data integrity throughout the blockchain
 */

void synchronize_blockchain(Node* node) {
    // Find the longest valid chain in the network
    int max_length = 0;
    Node* best_node = NULL;
    
    pthread_mutex_lock(&nodes_lock);
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_active && &nodes[i] != node) {
            pthread_mutex_lock(&nodes[i].chain->lock);
            if (nodes[i].chain->block_count > max_length) {
                max_length = nodes[i].chain->block_count;
                best_node = &nodes[i];
            }
            pthread_mutex_unlock(&nodes[i].chain->lock);
        }
    }
    
    // If we found a better chain, replace ours
    if (best_node) {
        pthread_mutex_lock(&best_node->chain->lock);
        pthread_mutex_lock(&node->chain->lock);
        
        // Free our current blockchain
        Block *current = node->chain->genesis, *next;
        while (current) {
            next = current->next;
            free_block(current);
            current = next;
        }
        
        // Clone the best chain
        current = best_node->chain->genesis;
        node->chain->genesis = clone_block(current);
        node->chain->block_count = 1;
        
        Block* prev = node->chain->genesis;
        current = current->next;
        
        while (current) {
            Block* new_block = clone_block(current);
            prev->next = new_block;
            prev = new_block;
            node->chain->block_count++;
            current = current->next;
        }
        
        node->chain->last_block = prev;
        
        // Create a new mining block
        if (node->chain->current_mining_block) {
            free_block(node->chain->current_mining_block);
        }
        node->chain->current_mining_block = create_block(node->chain->block_count, 
                                                         node->chain->last_block->hash);
        
        pthread_mutex_unlock(&node->chain->lock);
        pthread_mutex_unlock(&best_node->chain->lock);
        
        printf("Node %d synchronized with node %d (chain length: %d)\n", 
               node->id, best_node->id, max_length);
    }
    
    pthread_mutex_unlock(&nodes_lock);
}
// Validate an individual event/transaction
bool validate_event(Event* event) {
    //  this is supposed to check signatures, account balances... in real BC 
    // For this simulation, we just validate events of type 1 (transactions)
    if (event->type == 1) {
        // here assuming all transactions are valid ( is valid bool is set to true for all of them just for simulation )
        return true;
    }
    
    // Other event types don't need special validation
    return true;
}

// Validate all events in a block
bool validate_block_events(Block* block) {
    for (int i = 0; i < block->event_count; i++) {
        if (!validate_event(&block->events[i])) {
            return false;  // If any event is invalid, the block is invalid
        }
    }
    return true;  // All events are valid
}

// Add an event to a block
// Returns 1 on success, 0 if block is full
int add_event(Block* block, int type, const char* data) {
    if (block->event_count >= MAX_EVENTS) return 0;  // Block is full
    
    // Expand capacity if needed (dynamic resizing)
    if (block->event_count >= block->event_capacity) {
        block->event_capacity *= 2;  // Double the capacity
        
        // But we don't exceed MAX_EVENTS
        if (block->event_capacity > MAX_EVENTS) 
            block->event_capacity = MAX_EVENTS;
            
        // Reallocate the events array
        Event* new_events = realloc(block->events, block->event_capacity * sizeof(Event));
        if (!new_events) return 0;  // Memory allocation failed
        block->events = new_events;
    }
    
    // Add the new event to the block 
    Event* event = &block->events[block->event_count++];
    event->type = type;
    strncpy(event->data, data, sizeof(event->data) - 1);
    event->data[sizeof(event->data) - 1] = '\0';  // Ensure null termination
    
    // Set current timestamp
    time_t now = time(NULL);
    strftime(event->timestamp, sizeof(event->timestamp), 
             "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Calculate hash and validate
    hash_event(event);
    event->is_valid = validate_event(event);
    
    // Update block merkle root and hash
    calculate_merkle_root(block);
    hash_block(block);
    
    return 1;  // Success
}

/*
 * BLOCKCHAIN OPERATIONS
 * Functions for managing the entire blockchain
 */

// Create a new blockchain with a genesis block
Blockchain* create_blockchain() {
    Blockchain* chain = malloc(sizeof(Blockchain));
    
    // Create genesis block - the first block in the chain
    Block* genesis = create_block(0, "0000000000000000000000000000000000000000000000000000000000000000");
    calculate_merkle_root(genesis);
    hash_block(genesis);
    
    chain->genesis = genesis;
    chain->last_block = genesis;
    chain->block_count = 1;
    
    // Create the first mining block (will follow genesis)
    chain->current_mining_block = create_block(1, genesis->hash);
    
    // Initialize mutex for thread safety
    pthread_mutex_init(&chain->lock, NULL);
    
    return chain;
}

// Confirm a completed block and add it to the blockchain
void confirm_block(Blockchain* chain) {
    pthread_mutex_lock(&chain->lock);
    
    Block* new_block = chain->current_mining_block;
    
    // Finalize the block by calculating its merkle root and hash
    calculate_merkle_root(new_block);
    hash_block(new_block);
    
    // Add to the chain
    chain->last_block->next = new_block;
    chain->last_block = new_block;
    chain->block_count++;
    
    // Create a new mining block for future transactions
    chain->current_mining_block = create_block(chain->block_count, new_block->hash);
    
    pthread_mutex_unlock(&chain->lock);
}

// Add an event to the blockchain (to the current mining block)
int add_blockchain_event(Blockchain* chain, int type, const char* data) {
    pthread_mutex_lock(&chain->lock);
    
    //  add event to current mining block
    int result = add_event(chain->current_mining_block, type, data);
    
    // If block is full, confirm it and create a new one
    if (result == 0) {
        Block* old_block = chain->current_mining_block;
        
        // Create new mining block connected to the last confirmed block
        chain->current_mining_block = create_block(chain->block_count, 
                                                 chain->last_block->hash);
        
        pthread_mutex_unlock(&chain->lock);
        
        // Mine the full block (find a valid nonce)
        if (mine_block(old_block, DIFFICULTY)) {
            pthread_mutex_lock(&chain->lock);
            
            // Check if chain has changed while we were mining
            if (strcmp(chain->last_block->hash, old_block->previous_hash) == 0) {
                // Chain hasn't changed, we can add our block
                // This means no other node confirmed a block while we were mining
                chain->last_block->next = old_block;
                chain->last_block = old_block;
                chain->block_count++;
            } else {
                // Chain has changed (another node confirmed a block first)
                // We must discard our block to avoid a fork
                free_block(old_block);
            }
            
            pthread_mutex_unlock(&chain->lock);
        } else {
            // Mining was unsuccessful or interrupted
            free_block(old_block);
        }
        
        // Try adding the event again to the new mining block
        pthread_mutex_lock(&chain->lock);
        result = add_event(chain->current_mining_block, type, data);
        pthread_mutex_unlock(&chain->lock);
        
        return result;
    }
    
    pthread_mutex_unlock(&chain->lock);
    return result;
}

// Free all memory used by a blockchain
void free_blockchain(Blockchain* chain) {
    pthread_mutex_lock(&chain->lock);
    
    // Free all blocks in the chain
    Block *current = chain->genesis, *next;
    
    while (current) {
        next = current->next;
        free_block(current);
        current = next;
    }
    
    // Free mining block if not already in chain
    if (chain->current_mining_block != chain->last_block) {
        free_block(chain->current_mining_block);
    }
    
    pthread_mutex_unlock(&chain->lock);
    pthread_mutex_destroy(&chain->lock);
    
    free(chain);
}

/*
 * NODE OPERATIONS
 * Functions for managing blockchain network nodes
 */

// Get the latest confirmed block from a chain
Block* get_latest_block(Blockchain* chain) {
    pthread_mutex_lock(&chain->lock);
    Block* latest = chain->last_block;
    pthread_mutex_unlock(&chain->lock);
    return latest;
}

// Broadcast a new block to all other nodes in the network

void broadcast_block(Block* block, int sender_id) {
    pthread_mutex_lock(&nodes_lock);
    
    for (int i = 0; i < node_count; i++) {
        // Skip sender and inactive nodes
        if (nodes[i].id != sender_id && nodes[i].is_active) {
            pthread_mutex_lock(&nodes[i].chain->lock);
            
            // Check if block is valid 
            if (is_valid_proof(block, DIFFICULTY) && validate_block_events(block)) {
                // Check if this block builds on a block we have
                bool can_add = false;
                Block* current = nodes[i].chain->genesis;
                
                while (current) {
                    if (strcmp(current->hash, block->previous_hash) == 0) {
                        can_add = true;
                        break;
                    }
                    current = current->next;
                }
                
                if (can_add) {
                    // Block is valid and builds on our chain
                    Block* new_block = clone_block(block);
                    
                    // Check if this creates a longer chain
                    int new_chain_length = block->index + 1;
                    if (new_chain_length > nodes[i].chain->block_count) {
                        // This is a longer chain, update our last block
                        current->next = new_block;
                        nodes[i].chain->last_block = new_block;
                        nodes[i].chain->block_count = new_chain_length;
                        
                        // Update mining block to build on the new block
                        free_block(nodes[i].chain->current_mining_block);
                        nodes[i].chain->current_mining_block = create_block(nodes[i].chain->block_count, 
                                                                         new_block->hash);
                    }
                }
            }
            
            pthread_mutex_unlock(&nodes[i].chain->lock);
        }
    }
    
    pthread_mutex_unlock(&nodes_lock);
}
// Tamper with a transaction (malicious node behavior)
// This simulates an attack on the blockchain
void tamper_with_blockchain(Node* node) {
    if (!node->is_malicious || !node->is_active) return;
    
    pthread_mutex_lock(&node->chain->lock);
    
    // Find a block to tamper with 
    Block* current = node->chain->genesis->next;
    if (current == NULL) {
        pthread_mutex_unlock(&node->chain->lock);
        return;
    }
    
    // Attempt to modify a transaction in this block
    if (current->event_count > 0) {
        // Modify data of first transaction
        if (current->events[0].type == 1) {
            // Replace transaction data with fraudulent data
            strcpy(current->events[0].data, "{\"from\":\"System\",\"to\":\"Hacker\",\"amount\":1000}");
            
            // Recalculate event hash
            hash_event(&current->events[0]);
            
            printf("Node %d (malicious) tampered with transaction in block %d\n", 
                   node->id, current->index);
        }
    }
    
    pthread_mutex_unlock(&node->chain->lock);
}

// Calculate the longest chain among all nodes
// This is used for fork resolution - the longest valid chain wins
int get_longest_chain_length() {
    int max_length = 0;
    
    pthread_mutex_lock(&nodes_lock);
    
    // Check each active node's chain length
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_active) {
            pthread_mutex_lock(&nodes[i].chain->lock);
            if (nodes[i].chain->block_count > max_length) {
                max_length = nodes[i].chain->block_count;
            }
            pthread_mutex_unlock(&nodes[i].chain->lock);
        }
    }
    
    pthread_mutex_unlock(&nodes_lock);
    
    return max_length;
}

/*
 * NODE THREAD FUNCTION
 * The main operation loop for each blockchain node
 */

// Node thread function - handles mining and other operations
void* node_thread(void* arg) {
    Node* node = (Node*)arg;
    
    while (!shutdown_requested && node->is_active) {
        if (node->is_mining) {
            // Copy current mining block to work on independently
            pthread_mutex_lock(&node->chain->lock);
            Block* mining_block = clone_block(node->chain->current_mining_block);
            pthread_mutex_unlock(&node->chain->lock);
            
            // Mine the block (Proof of Work)
            bool success = mine_block(mining_block, DIFFICULTY);
            
            if (success && node->is_active) {
                printf("Node %d mined block %d with nonce %d: %s\n", 
                       node->id, mining_block->index, mining_block->nonce, mining_block->hash);
                
                pthread_mutex_lock(&node->chain->lock);
                
                // Ensure the chain hasn't changed while mining
                if (strcmp(node->chain->last_block->hash, mining_block->previous_hash) == 0) {
                    // Chain hasn't changed, we can add our block
                    // This means we won the mining race for this block
                    node->chain->last_block->next = mining_block;
                    node->chain->last_block = mining_block;
                    node->chain->block_count++;
                    
                    // Create new mining block
                    free_block(node->chain->current_mining_block);
                    node->chain->current_mining_block = create_block(node->chain->block_count, 
                                                                 mining_block->hash);
                    
                    pthread_mutex_unlock(&node->chain->lock);
                    
                    // Broadcast the new block to other nodes
                    broadcast_block(mining_block, node->id);
                } else {
                    // Chain has changed while we were mining
                    // Another node already mined a valid block, so discard ours
                    pthread_mutex_unlock(&node->chain->lock);
                    free_block(mining_block);
                }
            } else {
                // Mining failed or was interrupted
                free_block(mining_block);
            }
            
            // Malicious node behavior - occasionally try to tamper with the chain
            // This simulates an attack on the network
            if (node->is_malicious && rand() % 100 < 5) {  // 5% chance to attempt tampering
                tamper_with_blockchain(node);
            }
        }
        
        usleep(50000);  // 50ms pause to prevent CPU overload
    }
    
    return NULL;
}

/*
 * NODE MANAGEMENT FUNCTIONS
 * Create, start, and stop network nodes
 */

// Create a new blockchain node
Node* create_blockchain_node(bool is_mining, bool is_malicious) {
    pthread_mutex_lock(&nodes_lock);
    
    // Check if we have space for more nodes
    if (node_count >= MAX_NODES) {
        pthread_mutex_unlock(&nodes_lock);
        return NULL;
    }
    
    // Initialize the new node
    Node* node = &nodes[node_count];
    node->id = node_count++;
    node->chain = create_blockchain();  // Each node has its own copy of the blockchain
    node->is_mining = is_mining;        // Whether this node will mine new blocks
    node->is_malicious = is_malicious;  // Whether this node will try to cheat
    node->is_active = true;             // Node starts active
    
    pthread_mutex_unlock(&nodes_lock);
    
    // Start the node's processing thread
    pthread_create(&node->thread, NULL, node_thread, node);
    
    return node;
}

// Stop a node (take it offline) - to test the 4th test of availability apres
void stop_node(int node_id) {
    pthread_mutex_lock(&nodes_lock);
    
    // Validate node_id
    if (node_id < 0 || node_id >= node_count) {
        pthread_mutex_unlock(&nodes_lock);
        return;
    }
    
    // Mark the node as inactive
    nodes[node_id].is_active = false;
    
    pthread_mutex_unlock(&nodes_lock);
    
    // Wait for node's thread to terminate
    pthread_join(nodes[node_id].thread, NULL);
    
    printf("Node %d stopped\n", node_id);
}

// Start a previously stopped node (bring it back online)
void start_node(int node_id) {
    pthread_mutex_lock(&nodes_lock);
    
    // Validate node_id
    if (node_id < 0 || node_id >= node_count) {
        pthread_mutex_unlock(&nodes_lock);
        return;
    }
    
    // Only restart if it's currently inactive
    if (!nodes[node_id].is_active) {
        nodes[node_id].is_active = true;
        
        // Start a new processing thread for this node
        pthread_create(&nodes[node_id].thread, NULL, node_thread, &nodes[node_id]);
        
        printf("Node %d started\n", node_id);
        
        // Add synchronization after starting
        pthread_mutex_unlock(&nodes_lock);
        synchronize_blockchain(&nodes[node_id]);
        return;
    }
    
    pthread_mutex_unlock(&nodes_lock);
}

/*
 * CONSENSUS FUNCTIONS
 * Determine agreement across the network
 */

// Check if majority of active nodes have a particular block (consensus)
// This is how we determine if a block is accepted by the network
bool check_consensus(Block* block) {
    int total_active = 0;          // Total active nodes
    int nodes_with_block = 0;      // Nodes that have this block
    
    pthread_mutex_lock(&nodes_lock);
    
    // Count nodes that have this block in their chain
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_active) {
            total_active++;
            
            pthread_mutex_lock(&nodes[i].chain->lock);
            
            // Check if node has this block
            Block* current = nodes[i].chain->genesis;
            while (current) {
                if (strcmp(current->hash, block->hash) == 0) {
                    nodes_with_block++;
                    break;
                }
                current = current->next;
            }
            
            pthread_mutex_unlock(&nodes[i].chain->lock);
        }
    }
    
    pthread_mutex_unlock(&nodes_lock);
    
    // Check if consensus threshold is met (typically >50%)
    return (float)nodes_with_block / total_active >= CONSENSUS_THRESHOLD;
}

/*
 * DISPLAY FUNCTIONS
 * For visualizing blockchain state
 */

// Print details of a single block
void print_block(Block* block) {
    printf("Block #%d\n", block->index);
    printf("Time: %s", ctime(&block->timestamp));
    printf("Previous hash: %s\n", block->previous_hash);
    printf("Merkle root: %s\n", block->merkle_root);
    printf("Block hash: %s\n", block->hash);
    printf("Nonce: %d\n", block->nonce);
    printf("Events: %d\n", block->event_count);
    
    // Print all events in the block
    for (int i = 0; i < block->event_count; i++) {
        printf("  [%d] Type: %d | Valid: %s | Data: %s\n", 
               i+1, block->events[i].type, 
               block->events[i].is_valid ? "Yes" : "No", 
               block->events[i].data);
    }
    printf("\n");
}


// Print the entire blockchain
void print_blockchain(Blockchain* chain) {
    pthread_mutex_lock(&chain->lock);
    
    printf("=== BLOCKCHAIN (%d blocks) ===\n\n", chain->block_count);
    
    // Print each confirmed block
    Block* current = chain->genesis;
    while (current) {
        print_block(current);
        current = current->next;
    }
    
    // Print the block currently being mined
    printf("=== MINING BLOCK ===\n");
    print_block(chain->current_mining_block);
    
    pthread_mutex_unlock(&chain->lock);
}

// Print status information for a specific node
void print_node_status(int node_id) {
    pthread_mutex_lock(&nodes_lock);
    
    if (node_id < 0 || node_id >= node_count) {
        printf("Invalid node ID\n");
        pthread_mutex_unlock(&nodes_lock);
        return;
    }
    
    Node* node = &nodes[node_id];
    
    printf("=== NODE %d ===\n", node->id);
    printf("Status: %s\n", node->is_active ? "Active" : "Inactive");
    printf("Role: %s\n", node->is_mining ? "Miner" : "Validator");
    printf("Behavior: %s\n\n", node->is_malicious ? "Malicious" : "Honest");
    
    pthread_mutex_unlock(&nodes_lock);
    
    print_blockchain(node->chain);
}

/*
 * TEST FUNCTIONS
 * For validating blockchain functionality
 */

// Test nominal blockchain operations (read & insert)
void test_nominal_operations() {
    printf("\n=== TEST 1: NOMINAL OPERATIONS (READ & INSERT) ===\n");
    
    // Create honest mining nodes
    create_blockchain_node(true, false);  // Node 0: honest miner
    create_blockchain_node(true, false);  // Node 1: honest miner
    create_blockchain_node(false, false); // Node 2: validator only
    
    // Add some transactions
    add_blockchain_event(nodes[0].chain, 1, "{\"from\":\"Alice\",\"to\":\"Bob\",\"amount\":10}");
    sleep(1);  // Give time for propagation
    
    add_blockchain_event(nodes[1].chain, 1, "{\"from\":\"Bob\",\"to\":\"Carol\",\"amount\":5}");
    sleep(1);  // Give time for propagation
    
    print_node_status(0);
    
    // Check if transactions have propagated
    Block* latest_block_node0 = get_latest_block(nodes[0].chain);
    if (check_consensus(latest_block_node0)) {
        printf("TEST 1 PASSED: Consensus achieved on latest block\n");
    } else {
        printf("TEST 1 FAILED: No consensus on latest block\n");
    }
}

// Test unauthorized modifications to the blockchain
void test_unauthorized_modifications() {
    printf("\n=== TEST 2: UNAUTHORIZED MODIFICATIONS (UPDATE/DELETE) ===\n");
    
    // Create malicious node
    create_blockchain_node(true, true);   // Node 3: malicious miner
    
    // Wait for malicious behavior to happen
    sleep(2);
    
    // Check if malicious changes were accepted
    bool malicious_consensus = false;
    
    pthread_mutex_lock(&nodes[3].chain->lock);
    Block* malicious_block = nodes[3].chain->genesis->next;  // First non-genesis block
    pthread_mutex_unlock(&nodes[3].chain->lock);
    
    if (malicious_block) {
        malicious_consensus = check_consensus(malicious_block);
    }
    
    if (!malicious_consensus) {
        printf("TEST 2 PASSED: Unauthorized modifications rejected\n");
    } else {
        printf("TEST 2 FAILED: Unauthorized modifications accepted\n");
    }
}

// Test majority attack scenario (51% attack)
void test_majority_attack() {
    printf("\n=== TEST 3: MAJORITY ATTACK (51%%) ===\n");
    
    // Create more malicious nodes to have >50%
    create_blockchain_node(true, true);   // Node 4: malicious miner
    create_blockchain_node(true, true);   // Node 5: malicious miner
    
    // Now we have 3 malicious and 3 honest nodes (50/50)
    // Add one more malicious to make >50%
    create_blockchain_node(true, true);   // Node 6: malicious miner
    
    // Wait for malicious behavior to happen
    sleep(3);
    
    // Check if malicious chain is accepted
    int honest_chain_length = nodes[0].chain->block_count;
    int malicious_chain_length = nodes[3].chain->block_count;
    
    printf("Honest chain length: %d\n", honest_chain_length);
    printf("Malicious chain length: %d\n", malicious_chain_length);
    
    if (malicious_chain_length > honest_chain_length) {
        printf("TEST 3 RESULT: Majority attack successful (expected with >50%% malicious nodes)\n");
    } else {
        printf("TEST 3 RESULT: Majority attack unsuccessful\n");
    }
}

// Test node availability and recovery
void test_availability() {
    printf("\n=== TEST 4: AVAILABILITY (NODE FAILURE) ===\n");
    
    // Stop a node
    stop_node(0);
    
    // Add transaction to remaining node
    add_blockchain_event(nodes[1].chain, 1, "{\"from\":\"Dave\",\"to\":\"Eve\",\"amount\":15}");
    
    // Wait for propagation
    sleep(2);
    
    // Check blockchain state
    int chain_length_before = nodes[1].chain->block_count;
    
    // Restart node
    start_node(0);
    sleep(2); // Give time for synchronization
    
    // Check if restarted node caught up
    int chain_length_after = nodes[0].chain->block_count;
    
    printf("Chain length before restart: %d\n", chain_length_before);
    printf("Chain length after restart: %d\n", chain_length_after);
    
    if (chain_length_after >= chain_length_before) {
        printf("TEST 4 PASSED: Node recovered and synchronized\n");
    } else {
        printf("TEST 4 FAILED: Node failed to synchronize\n");
    }
}

/*
 * MAIN FUNCTION
 * Entry point for the blockchain simulation
 */

int main() {
    srand(time(NULL));  // Initialize random number generator
    
    // Run the test suite
    test_nominal_operations();
    test_unauthorized_modifications();
    test_majority_attack();
    test_availability();
    
    // Cleanup
    printf("\n=== SHUTTING DOWN BLOCKCHAIN ===\n");
    shutdown_requested = true;
    
    // Wait for threads to finish
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_active) {
            pthread_join(nodes[i].thread, NULL);
        }
        free_blockchain(nodes[i].chain);
    }
    
    printf("Blockchain simulation completed\n");
    return 0;
}
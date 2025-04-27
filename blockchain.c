#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_SIZE 64
#define MAX_EVENTS 100

// Core data structures - preserved from original
typedef struct {
    int type;
    char data[256];
    char timestamp[30];
    char hash[HASH_SIZE+1];
} Event;

typedef struct MerkleNode {
    char hash[HASH_SIZE+1];
    struct MerkleNode* left;
    struct MerkleNode* right;
} MerkleNode;

typedef struct Block {
    int index;
    time_t timestamp;
    char previous_hash[HASH_SIZE+1];
    Event* events;
    int event_count;
    int event_capacity;
    int nonce;
    char merkle_root[HASH_SIZE+1];
    char hash[HASH_SIZE+1];
    struct Block* next;
} Block;

typedef struct {
    Block* genesis;
    Block* last_block;
    int block_count;
    Block* current_mining_block;
} Blockchain;

// Simplified hash function
void hash_data(const char* input, char* output) {
    unsigned long hash = 5381;
    while (*input) hash = ((hash << 5) + hash) + *input++;
    sprintf(output, "%016lx", hash);
    
    // Fill with zeros to reach HASH_SIZE
    int len = strlen(output);
    memset(output + len, '0', HASH_SIZE - len);
    output[HASH_SIZE] = '\0';
}

// Event operations
void hash_event(Event* event) {
    char buffer[512];
    sprintf(buffer, "%d%s%s", event->type, event->data, event->timestamp);
    hash_data(buffer, event->hash);
}

// Merkle tree operations
MerkleNode* create_node(const char* hash) {
    MerkleNode* node = malloc(sizeof(MerkleNode));
    strcpy(node->hash, hash);
    node->left = node->right = NULL;
    return node;
}

void free_tree(MerkleNode* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

// Recursive merkle tree builder
MerkleNode* build_tree(char** hashes, int start, int end) {
    if (start > end) return NULL;
    if (start == end) return create_node(hashes[start]);
    
    int mid = (start + end) / 2;
    MerkleNode* left = build_tree(hashes, start, mid);
    MerkleNode* right = build_tree(hashes, mid + 1, end);
    
    // Handle odd number of nodes
    if (!right) right = create_node(left->hash);
    
    // Create parent node with combined hash
    MerkleNode* parent = create_node("");
    char combined[HASH_SIZE*2 + 1];
    sprintf(combined, "%s%s", left->hash, right->hash);
    hash_data(combined, parent->hash);
    
    parent->left = left;
    parent->right = right;
    return parent;
}

// Calculate merkle root for a block
void calculate_merkle_root(Block* block) {
    if (block->event_count == 0) {
        memset(block->merkle_root, '0', HASH_SIZE);
        block->merkle_root[HASH_SIZE] = '\0';
        return;
    }
    
    char** hashes = malloc(block->event_count * sizeof(char*));
    for (int i = 0; i < block->event_count; i++) {
        hashes[i] = block->events[i].hash;
    }
    
    MerkleNode* root = build_tree(hashes, 0, block->event_count - 1);
    strcpy(block->merkle_root, root->hash);
    
    free_tree(root);
    free(hashes);
}

// Block operations
void hash_block(Block* block) {
    char buffer[1024];
    sprintf(buffer, "%d%ld%s%s%d", 
            block->index, block->timestamp, 
            block->previous_hash, block->merkle_root, block->nonce);
    hash_data(buffer, block->hash);
}

Block* create_block(int index, const char* prev_hash) {
    Block* block = malloc(sizeof(Block));
    
    block->index = index;
    block->timestamp = time(NULL);
    strcpy(block->previous_hash, prev_hash);
    
    // Initialize events array
    block->event_capacity = 10;  // Start with space for 10 events
    block->events = malloc(block->event_capacity * sizeof(Event));
    block->event_count = 0;
    block->nonce = 0;
    block->next = NULL;
    
    return block;
}

void free_block(Block* block) {
    if (block) {
        free(block->events);
        free(block);
    }
}

// Add event to a block, return 1 on success, 0 if full
int add_event(Block* block, int type, const char* data) {
    if (block->event_count >= MAX_EVENTS) return 0;
    
    // Expand capacity if needed
    if (block->event_count >= block->event_capacity) {
        block->event_capacity *= 2;
        if (block->event_capacity > MAX_EVENTS) 
            block->event_capacity = MAX_EVENTS;
            
        Event* new_events = realloc(block->events, block->event_capacity * sizeof(Event));
        if (!new_events) return 0;
        block->events = new_events;
    }
    
    // Add the event
    Event* event = &block->events[block->event_count++];
    event->type = type;
    strncpy(event->data, data, sizeof(event->data) - 1);
    event->data[sizeof(event->data) - 1] = '\0';
    
    // Set timestamp
    time_t now = time(NULL);
    strftime(event->timestamp, sizeof(event->timestamp), 
             "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Calculate hash
    hash_event(event);
    
    // Update block hash
    calculate_merkle_root(block);
    hash_block(block);
    
    return 1;
}

// Blockchain operations
Blockchain* create_blockchain() {
    Blockchain* chain = malloc(sizeof(Blockchain));
    
    // Create genesis block
    Block* genesis = create_block(0, "0000000000000000000000000000000000000000000000000000000000000000");
    calculate_merkle_root(genesis);
    hash_block(genesis);
    
    chain->genesis = genesis;
    chain->last_block = genesis;
    chain->block_count = 1;
    chain->current_mining_block = create_block(1, genesis->hash);
    
    return chain;
}

void confirm_block(Blockchain* chain) {
    Block* new_block = chain->current_mining_block;
    
    // Finalize the block
    calculate_merkle_root(new_block);
    hash_block(new_block);
    
    // Add to chain
    chain->last_block->next = new_block;
    chain->last_block = new_block;
    chain->block_count++;
    
    // Create new mining block
    chain->current_mining_block = create_block(chain->block_count, new_block->hash);
}

// Add event to blockchain
int add_blockchain_event(Blockchain* chain, int type, const char* data) {
    int result = add_event(chain->current_mining_block, type, data);
    
    // If block full, confirm it and try again
    if (result == 0) {
        confirm_block(chain);
        result = add_event(chain->current_mining_block, type, data);
    }
    
    return result;
}

// Free blockchain memory
void free_blockchain(Blockchain* chain) {
    Block *current = chain->genesis, *next;
    
    while (current) {
        next = current->next;
        free_block(current);
        current = next;
    }
    
    // Free mining block if not in chain
    if (chain->current_mining_block != chain->last_block) {
        free_block(chain->current_mining_block);
    }
    
    free(chain);
}

// Display functions
void print_block(Block* block) {
    printf("Block #%d\n", block->index);
    printf("Time: %s", ctime(&block->timestamp));
    printf("Previous hash: %s\n", block->previous_hash);
    printf("Merkle root: %s\n", block->merkle_root);
    printf("Block hash: %s\n", block->hash);
    printf("Events: %d\n", block->event_count);
    
    for (int i = 0; i < block->event_count; i++) {
        printf("  [%d] Type: %d | Data: %s\n", 
               i+1, block->events[i].type, block->events[i].data);
    }
    printf("\n");
}

void print_blockchain(Blockchain* chain) {
    printf("=== BLOCKCHAIN (%d blocks) ===\n\n", chain->block_count);
    
    Block* current = chain->genesis;
    while (current) {
        print_block(current);
        current = current->next;
    }
    
    printf("=== MINING BLOCK ===\n");
    print_block(chain->current_mining_block);
}

// Interactive main function
int main() {
    Blockchain* chain = create_blockchain();
    
    // Add sample events
    add_blockchain_event(chain, 1, "{\"from\":\"System\",\"to\":\"Alice\",\"amount\":100}");
    add_blockchain_event(chain, 1, "{\"from\":\"System\",\"to\":\"Bob\",\"amount\":50}");
    add_blockchain_event(chain, 2, "{\"message\":\"Blockchain initialized\"}");
    
    confirm_block(chain);
    
    add_blockchain_event(chain, 1, "{\"from\":\"Alice\",\"to\":\"Bob\",\"amount\":10}");
    add_blockchain_event(chain, 3, "{\"action\":\"contract_execution\",\"id\":123}");
    
    print_blockchain(chain);
    free_blockchain(chain);
    
    return 0;
}
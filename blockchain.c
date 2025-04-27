#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_SIZE 64  // Taille d'un hash en caractères hexadécimaux
#define INITIAL_CAPACITY 10
#define MAX_EVENTS_PER_BLOCK 100  // Limite d'événements par bloc

// Structure générique d'un événement
typedef struct {
    int type;                // Type d'événement (1: transfert, 2: message, etc.)
    char data[256];          // Données de l'événement au format JSON ou autre
    char timestamp[30];      // Horodatage de l'événement
    char hash[HASH_SIZE+1];  // Hash de l'événement
} Event;

// Structure pour un nœud de l'arbre de Merkle
typedef struct MerkleNode {
    char hash[HASH_SIZE+1];
    struct MerkleNode* left;
    struct MerkleNode* right;
} MerkleNode;

// Structure d'un bloc
typedef struct Block {
    int index;                       // ID du bloc
    time_t timestamp;                // Horodatage de création
    char previous_hash[HASH_SIZE+1]; // Hash du bloc précédent
    Event* events;                   // Tableau dynamique d'événements
    int event_count;                 // Nombre d'événements dans le bloc
    int event_capacity;              // Capacité du tableau d'événements
    int nonce;                       // Nonce utilisé pour le minage
    char merkle_root[HASH_SIZE+1];   // Racine de l'arbre de Merkle
    char hash[HASH_SIZE+1];          // Hash du bloc courant
    struct Block* next;              // Pointeur vers le bloc suivant
} Block;

// Structure de la blockchain
typedef struct {
    Block* genesis;          // Premier bloc de la chaîne
    Block* last_block;       // Dernier bloc de la chaîne
    int block_count;         // Nombre de blocs dans la chaîne
    Block* current_mining_block; // Bloc en cours de minage (non confirmé)
} Blockchain;

// Forward declarations
void free_merkle_tree(MerkleNode* root);
void free_block(Block* block);

// Fonction pour libérer la mémoire d'un bloc
void free_block(Block* block) {
    if (block != NULL) {
        free(block->events);
        free(block);
    }
}

// Fonction simplifiée pour calculer un hash (à remplacer par une vraie fonction de hachage)
void calculate_hash_string(const char* input, char* output) {
    // Ceci est une simulation de hachage pour l'exemple
    // À remplacer par SHA-256 ou autre algorithme cryptographique
    
    // Pour l'exemple, nous utilisons une fonction très simple
    unsigned long hash = 5381;
    int c;
    const char* ptr = input;
    
    while ((c = *ptr++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    sprintf(output, "%016lx", hash);
    
    // Compléter le hash avec des zéros
    int len = strlen(output);
    for (int i = len; i < HASH_SIZE; i++) {
        output[i] = '0';
    }
    output[HASH_SIZE] = '\0';
}

// Fonction pour calculer le hash d'un événement
void calculate_event_hash(Event* event) {
    char buffer[512];
    sprintf(buffer, "%d%s%s", event->type, event->data, event->timestamp);
    calculate_hash_string(buffer, event->hash);
}

// Fonction pour créer un nœud de l'arbre de Merkle
MerkleNode* create_merkle_node(const char* hash) {
    MerkleNode* node = (MerkleNode*)malloc(sizeof(MerkleNode));
    if (node == NULL) {
        fprintf(stderr, "Erreur d'allocation mémoire pour un nœud de Merkle\n");
        exit(EXIT_FAILURE);
    }
    
    strcpy(node->hash, hash);
    node->left = NULL;
    node->right = NULL;
    
    return node;
}

// Fonction pour libérer un arbre de Merkle
void free_merkle_tree(MerkleNode* root) {
    if (root == NULL) return;
    
    // On sauvegarde les enfants avant de libérer le parent
    MerkleNode* left = root->left;
    MerkleNode* right = root->right;
    
    // Libérer d'abord le nœud actuel
    free(root);
    
    // Puis libérer récursivement les enfants
    if (left) free_merkle_tree(left);
    if (right) free_merkle_tree(right);
}

// Fonction pour calculer le hash d'un nœud parent dans l'arbre de Merkle
void calculate_parent_hash(const char* left_hash, const char* right_hash, char* parent_hash) {
    char combined[HASH_SIZE*2 + 1];
    sprintf(combined, "%s%s", left_hash, right_hash);
    calculate_hash_string(combined, parent_hash);
}

// Correction dans la fonction build_merkle_tree
MerkleNode* build_merkle_tree(char** event_hashes, int count) {
    if (count == 0) return NULL;
    
    // Créer un tableau de nœuds de feuilles
    MerkleNode** nodes = (MerkleNode**)malloc(count * sizeof(MerkleNode*));
    for (int i = 0; i < count; i++) {
        nodes[i] = create_merkle_node(event_hashes[i]);
    }
    
    // S'il n'y a qu'un seul événement, retourner son nœud directement
    if (count == 1) {
        MerkleNode* root = nodes[0];
        free(nodes);
        return root;
    }
    
    int current_level_size = count;
    int next_level_size;
    
    // Construire l'arbre niveau par niveau
    while (current_level_size > 1) {
        next_level_size = (current_level_size + 1) / 2;  // Arrondir vers le haut
        
        for (int i = 0; i < next_level_size; i++) {
            int left_idx = i * 2;
            int right_idx = left_idx + 1;
            
            MerkleNode* left = nodes[left_idx];
            // CORRECTION: Ne pas dupliquer le nœud mais créer une copie du hash
            MerkleNode* right;
            if (right_idx < current_level_size) {
                right = nodes[right_idx];
            } else {
                // Utiliser le même hash mais créer un nouveau nœud
                right = create_merkle_node(left->hash);
            }
            
            MerkleNode* parent = create_merkle_node("");
            calculate_parent_hash(left->hash, right->hash, parent->hash);
            
            parent->left = left;
            parent->right = right;
            
            nodes[i] = parent;
        }
        
        current_level_size = next_level_size;
    }
    
    // Le premier nœud est maintenant la racine
    MerkleNode* root = nodes[0];
    free(nodes);
    return root;
}

// Fonction pour calculer la racine de Merkle d'un bloc - avec correction
void calculate_merkle_root(Block* block) {
    if (block->event_count == 0) {
        // Cas spécial: pas d'événements
        memset(block->merkle_root, '0', HASH_SIZE);
        block->merkle_root[HASH_SIZE] = '\0';
        return;
    }
    
    // Extraire les hashes des événements
    char** event_hashes = (char**)malloc(block->event_count * sizeof(char*));
    for (int i = 0; i < block->event_count; i++) {
        event_hashes[i] = block->events[i].hash;
    }
    
    // Construire l'arbre de Merkle
    MerkleNode* root = build_merkle_tree(event_hashes, block->event_count);
    
    // Copier le hash de la racine
    strcpy(block->merkle_root, root->hash);
    
    // Libérer la mémoire
    free_merkle_tree(root);
    free(event_hashes);
}

// Alternative: version non récursive pour éviter les problèmes de pile
void free_merkle_tree_iterative(MerkleNode* root) {
    if (root == NULL) return;
    
    // Créer une pile pour stocker les nœuds à libérer
    MerkleNode** stack = (MerkleNode**)malloc(1000 * sizeof(MerkleNode*)); // Taille arbitraire
    int top = 0;
    
    // Ajouter la racine à la pile
    stack[top++] = root;
    
    while (top > 0) {
        // Prendre un nœud de la pile
        MerkleNode* node = stack[--top];
        
        // Ajouter ses enfants à la pile s'ils existent
        if (node->right) stack[top++] = node->right;
        if (node->left) stack[top++] = node->left;
        
        // Libérer le nœud
        free(node);
    }
    
    free(stack);
}

// Fonction pour libérer la mémoire de la blockchain - avec correction
void free_blockchain(Blockchain* blockchain) {
    if (blockchain == NULL) {
        return;
    }
    
    Block* current = blockchain->genesis;
    
    while (current != NULL) {
        Block* next = current->next;
        free_block(current);
        current = next;
    }
    
    // Vérifier si le bloc en cours de minage n'est pas déjà dans la chaîne
    // (il ne doit pas l'être normalement, mais on vérifie par sécurité)
    Block* last = blockchain->last_block;
    Block* mining = blockchain->current_mining_block;
    
    if (mining != NULL && (last == NULL || mining != last)) {
        free_block(mining);
    }
    
    free(blockchain);
}

// Fonction pour calculer le hash d'un bloc
void calculate_block_hash(Block* block) {
    char buffer[1024];
    sprintf(buffer, "%d%ld%s%s%d", 
            block->index, 
            block->timestamp, 
            block->previous_hash, 
            block->merkle_root, 
            block->nonce);
    
    calculate_hash_string(buffer, block->hash);
}

// Fonction pour créer un nouveau bloc
Block* create_block(int index, const char* previous_hash) {
    Block* block = (Block*)malloc(sizeof(Block));
    if (block == NULL) {
        fprintf(stderr, "Erreur d'allocation mémoire pour le bloc\n");
        exit(EXIT_FAILURE);
    }
    
    block->index = index;
    block->timestamp = time(NULL);
    strcpy(block->previous_hash, previous_hash);
    
    // Initialiser le tableau dynamique d'événements
    block->event_capacity = INITIAL_CAPACITY;
    block->events = (Event*)malloc(block->event_capacity * sizeof(Event));
    if (block->events == NULL) {
        fprintf(stderr, "Erreur d'allocation mémoire pour les événements\n");
        free(block);
        exit(EXIT_FAILURE);
    }
    
    block->event_count = 0;
    block->nonce = 0;
    block->next = NULL;
    
    // Initialiser le hash et la racine de Merkle
    memset(block->hash, 0, HASH_SIZE+1);
    memset(block->merkle_root, 0, HASH_SIZE+1);
    
    return block;
}

// Fonction pour ajouter un événement à un bloc
int add_event_to_block(Block* block, int type, const char* data) {
    // Vérifier si le bloc est plein
    if (block->event_count >= MAX_EVENTS_PER_BLOCK) {
        return 0;  // Bloc plein
    }
    
    // Vérifier si le tableau d'événements doit être redimensionné
    if (block->event_count >= block->event_capacity) {
        int new_capacity = block->event_capacity * 2;
        if (new_capacity > MAX_EVENTS_PER_BLOCK) {
            new_capacity = MAX_EVENTS_PER_BLOCK;
        }
        
        Event* new_events = (Event*)realloc(block->events, new_capacity * sizeof(Event));
        if (new_events == NULL) {
            return 0;  // Échec du redimensionnement
        }
        block->events = new_events;
        block->event_capacity = new_capacity;
    }
    
    // Ajouter le nouvel événement
    Event* event = &block->events[block->event_count];
    event->type = type;
    strncpy(event->data, data, sizeof(event->data) - 1);
    event->data[sizeof(event->data) - 1] = '\0';  // Assurer la terminaison
    
    // Horodatage de l'événement
    time_t now = time(NULL);
    strftime(event->timestamp, sizeof(event->timestamp), 
             "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Calculer le hash de l'événement
    calculate_event_hash(event);
    
    block->event_count++;
    
    // Recalculer la racine de Merkle et le hash du bloc
    calculate_merkle_root(block);
    calculate_block_hash(block);
    
    return 1;  // Succès
}

// Fonction pour créer une nouvelle blockchain
Blockchain* create_blockchain() {
    Blockchain* blockchain = (Blockchain*)malloc(sizeof(Blockchain));
    if (blockchain == NULL) {
        fprintf(stderr, "Erreur d'allocation mémoire pour la blockchain\n");
        exit(EXIT_FAILURE);
    }
    
    // Créer le bloc genesis
    Block* genesis = create_block(0, "0000000000000000000000000000000000000000000000000000000000000000");
    calculate_merkle_root(genesis);
    calculate_block_hash(genesis);
    
    blockchain->genesis = genesis;
    blockchain->last_block = genesis;
    blockchain->block_count = 1;
    blockchain->current_mining_block = create_block(1, genesis->hash);
    
    return blockchain;
}

// Fonction pour confirmer un bloc (finaliser le minage)
void confirm_block(Blockchain* blockchain) {
    // Ajouter le bloc à la chaîne
    Block* new_block = blockchain->current_mining_block;
    
    // Finaliser le bloc
    calculate_merkle_root(new_block);
    calculate_block_hash(new_block);
    
    // Mettre à jour les liens
    blockchain->last_block->next = new_block;
    blockchain->last_block = new_block;
    blockchain->block_count++;
    
    // Créer un nouveau bloc pour le minage
    blockchain->current_mining_block = create_block(blockchain->block_count, new_block->hash);
}

// Fonction pour ajouter un événement à la blockchain
int add_event_to_blockchain(Blockchain* blockchain, int type, const char* data) {
    // Essayer d'ajouter l'événement au bloc en cours de minage
    int result = add_event_to_block(blockchain->current_mining_block, type, data);
    
    // Si le bloc est plein, confirmer le bloc et créer un nouveau
    if (result == 0) {
        confirm_block(blockchain);
        result = add_event_to_block(blockchain->current_mining_block, type, data);
    }
    
    return result;
}

// Fonction pour afficher un bloc
void print_block(const Block* block) {
    printf("Bloc #%d\n", block->index);
    printf("Timestamp: %s", ctime(&block->timestamp));
    printf("Hash du bloc précédent: %s\n", block->previous_hash);
    printf("Racine de Merkle: %s\n", block->merkle_root);
    printf("Hash du bloc: %s\n", block->hash);
    printf("Nonce: %d\n", block->nonce);
    printf("Événements (%d/%d):\n", block->event_count, MAX_EVENTS_PER_BLOCK);
    
    for (int i = 0; i < block->event_count; i++) {
        printf("  Événement %d:\n", i+1);
        printf("    Type: %d\n", block->events[i].type);
        printf("    Données: %s\n", block->events[i].data);
        printf("    Horodatage: %s\n", block->events[i].timestamp);
        printf("    Hash: %s\n", block->events[i].hash);
    }
    printf("\n");
}

// Fonction pour afficher la blockchain
void print_blockchain(const Blockchain* blockchain) {
    printf("Blockchain (%d blocs):\n", blockchain->block_count);
    
    Block* current = blockchain->genesis;
    while (current != NULL) {
        print_block(current);
        current = current->next;
    }
    
    printf("Bloc en cours de minage:\n");
    print_block(blockchain->current_mining_block);
}

// Exemple d'utilisation
int main() {
    // Créer une nouvelle blockchain
    Blockchain* blockchain = create_blockchain();
    
    // Ajouter des événements à la blockchain
    add_event_to_blockchain(blockchain, 1, "{\"from\":\"System\",\"to\":\"Alice\",\"amount\":100}");
    add_event_to_blockchain(blockchain, 1, "{\"from\":\"System\",\"to\":\"Bob\",\"amount\":50}");
    add_event_to_blockchain(blockchain, 2, "{\"message\":\"Blockchain initialized\"}");
    
    // Confirmer le premier bloc
    confirm_block(blockchain);
    
    // Ajouter d'autres événements
    add_event_to_blockchain(blockchain, 1, "{\"from\":\"Alice\",\"to\":\"Bob\",\"amount\":10}");
    add_event_to_blockchain(blockchain, 3, "{\"action\":\"contract_execution\",\"contract_id\":123}");
    
    // Afficher la blockchain
    print_blockchain(blockchain);
    
    // Libérer la mémoire
    free_blockchain(blockchain);
    
    return 0;
}
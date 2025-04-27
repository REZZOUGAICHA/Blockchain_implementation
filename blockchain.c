#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_SIZE 64  // Taille d'un hash en caractères hexadécimaux
#define INITIAL_CAPACITY 10

// Structure générique d'un événement (remplace la Transaction spécifique)
typedef struct {
    int type;                // Type d'événement (1: transfert, 2: message, etc.)
    char data[256];          // Données de l'événement au format JSON ou autre
    char timestamp[30];      // Horodatage de l'événement
} Event;

// Structure d'un bloc
typedef struct Block {
    int index;                      // ID du bloc
    time_t timestamp;               // Horodatage de création
    char previous_hash[HASH_SIZE+1]; // Hash du bloc précédent
    Event* events;                  // Tableau dynamique d'événements
    int event_count;                // Nombre d'événements dans le bloc
    int event_capacity;             // Capacité du tableau d'événements
    int nonce;                      // Nonce utilisé pour le minage
    char hash[HASH_SIZE+1];         // Hash du bloc courant
    struct Block* next;             // Pointeur vers le bloc suivant (optionnel)
} Block;

// Structure de la blockchain
typedef struct {
    Block* genesis;          // Premier bloc de la chaîne
    Block* last_block;       // Dernier bloc de la chaîne
    int block_count;         // Nombre de blocs dans la chaîne
} Blockchain;

// Fonction simplifiée pour calculer un hash (à remplacer par une fonction de hachage réelle)
void calculate_hash(Block* block) {
    // Remplacez cette implémentation par une fonction de hachage réelle
    // Ceci est juste une simulation basique pour l'exemple
    
    // Générer un hash aléatoire pour l'exemple
    sprintf(block->hash, "hash%d_%ld", block->index, block->timestamp);
    
    // Complétez le hash jusqu'à HASH_SIZE caractères
    int len = strlen(block->hash);
    for (int i = len; i < HASH_SIZE; i++) {
        block->hash[i] = '0';
    }
    block->hash[HASH_SIZE] = '\0';
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
    
    // Initialiser le hash
    memset(block->hash, 0, HASH_SIZE+1);
    
    return block;
}

// Fonction pour ajouter un événement à un bloc
int add_event(Block* block, int type, const char* data) {
    // Vérifier si le tableau d'événements doit être redimensionné
    if (block->event_count >= block->event_capacity) {
        block->event_capacity *= 2;
        Event* new_events = (Event*)realloc(block->events, block->event_capacity * sizeof(Event));
        if (new_events == NULL) {
            return 0;  // Échec du redimensionnement
        }
        block->events = new_events;
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
    
    block->event_count++;
    
    // Recalculer le hash du bloc après ajout de l'événement
    calculate_hash(block);
    
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
    calculate_hash(genesis);
    
    blockchain->genesis = genesis;
    blockchain->last_block = genesis;
    blockchain->block_count = 1;
    
    return blockchain;
}

// Fonction pour ajouter un bloc à la blockchain
void add_block_to_blockchain(Blockchain* blockchain, Block* new_block) {
    // Mettre à jour les liens de la chaîne
    blockchain->last_block->next = new_block;
    blockchain->last_block = new_block;
    blockchain->block_count++;
    
    // Calculer le hash final du nouveau bloc
    calculate_hash(new_block);
}

// Fonction pour créer et ajouter un nouveau bloc à la blockchain
Block* mine_block(Blockchain* blockchain) {
    // Créer un nouveau bloc
    Block* new_block = create_block(blockchain->block_count, blockchain->last_block->hash);
    
    // Ajouter le bloc à la blockchain
    add_block_to_blockchain(blockchain, new_block);
    
    return new_block;
}

// Fonction pour afficher un bloc
void print_block(const Block* block) {
    printf("Bloc #%d\n", block->index);
    printf("Timestamp: %s", ctime(&block->timestamp));
    printf("Hash du bloc précédent: %s\n", block->previous_hash);
    printf("Hash du bloc: %s\n", block->hash);
    printf("Nonce: %d\n", block->nonce);
    printf("Événements (%d):\n", block->event_count);
    
    for (int i = 0; i < block->event_count; i++) {
        printf("  Événement %d:\n", i+1);
        printf("    Type: %d\n", block->events[i].type);
        printf("    Données: %s\n", block->events[i].data);
        printf("    Horodatage: %s\n", block->events[i].timestamp);
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
}

// Fonction pour libérer la mémoire d'un bloc
void free_block(Block* block) {
    if (block != NULL) {
        free(block->events);
        free(block);
    }
}

// Fonction pour libérer la mémoire de la blockchain
void free_blockchain(Blockchain* blockchain) {
    if (blockchain == NULL) {
        return;
    }
    
    Block* current = blockchain->genesis;
    Block* next;
    
    while (current != NULL) {
        next = current->next;
        free_block(current);
        current = next;
    }
    
    free(blockchain);
}

// Exemple d'utilisation
int main() {
    // Créer une nouvelle blockchain
    Blockchain* blockchain = create_blockchain();
    
    // Ajouter des événements au bloc genesis
    add_event(blockchain->genesis, 1, "{\"from\":\"System\",\"to\":\"Alice\",\"amount\":100}");
    add_event(blockchain->genesis, 1, "{\"from\":\"System\",\"to\":\"Bob\",\"amount\":50}");
    add_event(blockchain->genesis, 2, "{\"message\":\"Blockchain initialized\"}");
    
    // Créer un nouveau bloc
    Block* block1 = mine_block(blockchain);
    
    // Ajouter des événements au bloc 1
    add_event(block1, 1, "{\"from\":\"Alice\",\"to\":\"Bob\",\"amount\":10}");
    add_event(block1, 3, "{\"action\":\"contract_execution\",\"contract_id\":123}");
    
    // Afficher la blockchain
    print_blockchain(blockchain);
    
    // Libérer la mémoire
    free_blockchain(blockchain);
    
    return 0;
}
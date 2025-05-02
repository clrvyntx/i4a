#include "shared_memory.h"

void init_shared_memory(SharedMemory* shared_memory) {
    shared_memory->size = 0;

    for (int i = 0; i < TABLE_SIZE; i++) {
        shared_memory->buckets[i].key = 0;
        shared_memory->buckets[i].value[0] = '\0'; // Para asegurarnos que todos se inicializan bien
    }
}

void save_node_variable(SharedMemory* shared_memory, const char *msg, uint16_t key) {
    if (shared_memory->size < TABLE_SIZE) {
        Node* new_node = &shared_memory->buckets[shared_memory->size];;
        strncpy(new_node->value, msg, sizeof(new_node->value) - 1);
        new_node->value[VALUE_SIZE - 1] = '\0'; // Para asegurarnos que todos terminan asi
        new_node->key = key;
        shared_memory->size++;
        shared_memory->buckets[shared_memory->size] = *new_node;
    } else {
        printf("Error: No more space in shared memory\n");
    }
}

Node* get_node_variable(SharedMemory* shared_memory, uint16_t key) {
    for (int i = 0; i < shared_memory->size; i++) { // TODO: podriamos usar una funcion de hashing
        if (shared_memory->buckets[i].key == key) {
            return &shared_memory->buckets[i];
        }
    }
    return NULL;
}
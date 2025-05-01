#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 10
#define VALUE_SIZE 128

typedef struct Node {
    char value[VALUE_SIZE];
    uint16_t key;
} Node;

typedef struct SharedMemory {
    Node buckets[TABLE_SIZE];
    int size;
} SharedMemory;


void init_shared_memory(SharedMemory* shared_memory);
void save_node_variable(SharedMemory* shared_memory,const char* msg, uint16_t key);
Node* get_node_variable(SharedMemory* shared_memory, uint16_t key);
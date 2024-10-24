#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>


pthread_mutex_t memory_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_memory_blocks();
void debug_memory_pool();

// MemoryBlock structure
typedef struct MemoryBlock {
    size_t size; // Size in bytes
    int free; // Indicates if the block is free, 1 for True, 0 for False
    void *data; // Pointer to the actual data
    struct MemoryBlock *next; // Pointer to the next MemoryBlock
} MemoryBlock;

// Keeps track of the metadata of the memory block.
#define BLOCK_SIZE sizeof(MemoryBlock)

// Pointer to the head of the linked list of memory blocks
MemoryBlock *global_block = NULL;

// Size of current memory pool
size_t pool_size;
size_t used_size = 0;

// Initializes the memory pool with the given size
void mem_init(size_t size) {
    pthread_mutex_lock(&memory_mutex);
    // Allocate the memory for the memory block itself.
    global_block = (MemoryBlock*) malloc(BLOCK_SIZE);

    if(global_block == NULL){
        // If the allocation fails, exit
        printf("Failed to allocate global_block");
        pthread_mutex_unlock(&memory_mutex);
        return;
    }
    
    // Set the size for the global block, set it to be free and add a null pointer to indicate it is the last in the list (for now).
    pool_size = size;
    global_block->size = size;
    global_block->data = malloc(size); // Allocate the size we want to use.
    global_block->free = 1;
    global_block->next = NULL;
    pthread_mutex_unlock(&memory_mutex);
}

// Helper Function for mem_alloc
MemoryBlock* find_free_block(size_t size) {
    MemoryBlock *current = global_block;

    // Go through each memory_block until a free block with enough space is found
    while (current) {      
        if (current->free == 1 && current->size >= size){
            return current; 
        }
        current = current->next;
    }
    // If no suitable block is found, return NULL
    return NULL;
}

// Helper function to check total available memory.
size_t calculate_available_memory() {
    size_t total_free_memory = 0;
    MemoryBlock *current = global_block;

    // Iterate through all blocks to calculate total free memory
    while (current != NULL) {
        if (current->free == 1) {
            total_free_memory += current->size;
        }
        current = current->next;
    }

    return total_free_memory;
}

// Helper function to check current memory usage
size_t calculate_used_memory(){
    size_t used_memory = 0;
    MemoryBlock *current = global_block;

    // Iterate through all blocks to calculate total used memory
    while (current != NULL) {
        if (current->free == 0) {
            used_memory += current->size;
        }
        current = current->next;
    }

    return used_memory;
}

// Allocates requested size to a memory_block and puts the remaining space into a new_block 
void* mem_alloc_internal(size_t size){

    size_t used_memory = calculate_used_memory();
    size_t available_memory = calculate_available_memory();

    if(size > available_memory){
        return NULL;
    }

    if(used_memory + size > pool_size){
        return NULL;
    }

    if(size == 0){
        return global_block;
    }

    // Find a free memory block
    MemoryBlock *block = find_free_block(size);    
    
    // If requested size is 0, return pointer to the metadata.
    if(!block) {
        return NULL; 
    }

    // Mark the block as allocated.
    block->free = 0;

    // Split the block if it's larger than needed
    if(block->size > size){
        MemoryBlock *new_block = (MemoryBlock*) malloc(BLOCK_SIZE);
        new_block->size = block->size - size; // Set size of the new block to be the remaining space
        new_block->data = (char*)block->data + size; // Allocate new_block->data to be after current block->data
        new_block->free = 1;                // Mark it as free
        new_block->next = block->next;     // Link the new block to the next

        block->size = size;               // Adjust the current block size
        block->next = new_block;         // Have the current block point to the new one.
        used_size += size;
    }

    // Return a pointer to the usable memory (after the metadata)
    return block->data;
}

// Public Function, calls the internal memory_allocator.
void* mem_alloc(size_t size){
    pthread_mutex_lock(&memory_mutex);
    void* block_ptr = mem_alloc_internal(size);
    pthread_mutex_unlock(&memory_mutex);
    return block_ptr;
}

// Helper function for mem_free
MemoryBlock* get_block_ptr(void *data_ptr) {
    MemoryBlock *current = global_block;

    // Iterate through the linked list of blocks to find the matching data pointer
    while (current != NULL) {
        if (current->data == data_ptr) {
            return current;
        }
        current = current->next;
    }

    return NULL; // Return NULL if no matching block is found
}

// Function to free allocated blocks
void mem_free_internal(void* block) {
    // If no block given, simply go back
    if (!block){
        return;
    }
    
    // Get the pointer for the metadata
    MemoryBlock *block_ptr = get_block_ptr(block);
    if (!block_ptr) {
        return;
    }
    // printf("Freeing block: %p, size: %zu\n", block_ptr, block_ptr->size);

    block_ptr->free = 1; // Mark it as free
    used_size -= block_ptr->size;

    // Merge with the next block if it's free
    if (block_ptr->next && block_ptr->next->free) {
        block_ptr->size += block_ptr->next->size; // Combine sizes
        MemoryBlock *temp = block_ptr->next; // Save the block being removed
        block_ptr->next = block_ptr->next->next; // Link to the next of the next block
        free(temp); // Free the memory of the merged block
    }
}

// Public function, calls mem_free_internal
void mem_free(void* block){
    pthread_mutex_lock(&memory_mutex);
    mem_free_internal(block);
    pthread_mutex_unlock(&memory_mutex);
}

// Resizes the given block to the requested size
void* mem_resize(void* block, size_t size){
    // If block is NULL, allocate a new block
    pthread_mutex_lock(&memory_mutex);
    if (!block){    
        MemoryBlock* new_block = mem_alloc_internal(size);
        if(!new_block){
            pthread_mutex_unlock(&memory_mutex);
            return NULL;
        }
        pthread_mutex_unlock(&memory_mutex);
        return new_block;
    }

    // Get the block
    MemoryBlock *block_ptr = get_block_ptr(block);
    
    // If the block is already big enough, return the same block
    if(block_ptr->size == size){
        pthread_mutex_unlock(&memory_mutex);
        return block;
    }
    
    // Allocate a new block of the requested size
    void *new_block_ptr = mem_alloc_internal(size);

    // If allocation fails, return NULL
    if(!new_block_ptr){
        pthread_mutex_unlock(&memory_mutex);
        return NULL;
    }

    // Copy the data from the old block to the new.
    memcpy(new_block_ptr, block, block_ptr->size);
    mem_free_internal(block); // Free old block
    pthread_mutex_unlock(&memory_mutex);
    return new_block_ptr;
}

// Frees the entire memory pool and deinitializes the memory pool
void mem_deinit(){
    pthread_mutex_lock(&memory_mutex);
    MemoryBlock *current = global_block;
    MemoryBlock *next;

    free(global_block->data);

    while (current != NULL) {
        next = current->next;  // Save pointer to the next block
        
        if (!next){
            free(current);
            break;
        }

        if (current) {
            free(current);  // Free the data if allocated separately
        }
        current = next;  // Move to the next block
    }
    
    global_block = NULL;  // Set the head to NULL
    pthread_mutex_unlock(&memory_mutex);
}

// Purely for debugging purposes
void print_memory_blocks() {
    MemoryBlock *current = global_block; // Start from the head of the list

    // Print the header
    printf("Current Memory Blocks:\n");
    printf("%-20s %-10s %-10s %-10s\n", "Address", "Size", "Free", "Data Ptr");
    printf("----------------------------------------------\n");

    // Iterate through the list of memory blocks and print details
    while (current != NULL) {
        printf("%-20p %-10zu %-10d %-10p\n",
               (void*)current, // Address of the memory block
               current->size,  // Size of the block
               current->free,  // Whether the block is free (1 for true, 0 for false)
               current->data); // Pointer to the data
        current = current->next; // Move to the next block
    }
}

void debug_memory_pool() {
    MemoryBlock *current = global_block; // Assuming `memory_head` is the head of your memory pool list.
    int block_index = 0;

    printf("====== Memory Pool Debugging Info ======\n");
    while (current != NULL) {
        printf("Block %d: Address: %p, Size: %zu, Free: %d, Data Address: %p\n", 
               block_index, 
               (void*)current, 
               current->size, 
               current->free, 
               (void*)current->data);

        // Move to the next block
        current = current->next;
        block_index++;
    }
    printf("========================================\n");
}
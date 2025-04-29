#include "buddy.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BLOCKS (TOTAL_MEMORY / MIN_BLOCK)

Block** memory;

void init_buddy() {
    memory = (Block**)malloc(sizeof(Block*) * MAX_BLOCKS);
    if (!memory) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }
    
    // Initialize all memory blocks
    for (int i = 0; i < MAX_BLOCKS; i++) {
        memory[i] = (Block*)malloc(sizeof(Block));
        if (!memory[i]) {
            // Clean up already allocated memory
            for (int j = 0; j < i; j++) {
                free(memory[j]);
            }
            free(memory);
            memory = NULL;
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return;
        }
        // Initialize all blocks properly
        memory[i]->size = 0;
        memory[i]->is_free = 0;
        memory[i]->pid = -1;
    }
    
    // Set up initial block
    memory[0]->size = TOTAL_MEMORY;
    memory[0]->is_free = 1;
    memory[0]->pid = -1;
}

int allocate_memory(int pid, int size) {
    if (size <= 0) {
        return -1;  // Invalid size
    }
    
    // Calculate needed size (round up to nearest power of 2)
    int needed_size = MIN_BLOCK;
    while (needed_size < size) {
        needed_size *= 2;
        if (needed_size > TOTAL_MEMORY) {
            return -1;  // Requested size too large
        }
    }
    
    // Find a suitable block
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (memory[i]->is_free && memory[i]->size >= needed_size) {
            // Split the block until it's the right size
            while (memory[i]->size > needed_size) {
                if (split_block(i) == 0) {
                    return -1;  // Split failed
                }
            }
            
            // Allocate the block
            memory[i]->is_free = 0;
            memory[i]->pid = pid;
            return i * MIN_BLOCK;  // Return the memory address
        }
    }
    
    return -1;  // No suitable block found
}

int split_block(int index) {
    if (index < 0 || index >= MAX_BLOCKS || memory[index]->size == MIN_BLOCK) {
        return 0;  // Can't split this block
    }
    
    int half_size = memory[index]->size / 2;
    int buddy_index = find_free_index();
    
    if (buddy_index == -1) {
        return 0;  // No free blocks available
    }
    
    // Calculate the buddy's position (should be at offset half_size from current block)
    int buddy_offset = index + (half_size / MIN_BLOCK);
    
    // If the buddy's position is already taken, use the free index we found
    if (buddy_offset < MAX_BLOCKS && memory[buddy_offset]->size == 0) {
        buddy_index = buddy_offset;
    }
    
    // Initialize the buddy block
    memory[buddy_index]->size = half_size;
    memory[buddy_index]->is_free = 1;
    memory[buddy_index]->pid = -1;
    
    // Update the original block
    memory[index]->size = half_size;
    
    return 1;  // Split successful
}

int find_free_index() {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (memory[i]->size == 0) {
            return i;
        }
    }
    return -1;  // No free blocks
}

void free_memory(int pid) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (memory[i]->pid == pid) {
            memory[i]->is_free = 1;
            memory[i]->pid = -1;
            merge_buddies(i);
            return;
        }
    }
}

void merge_buddies(int index) {
    if (index < 0 || index >= MAX_BLOCKS) {
        return;  // Invalid index
    }
    
    while (1) {
        int size = memory[index]->size;
        if (size == TOTAL_MEMORY) {
            break;  // Can't merge anymore
        }
        
        // Calculate buddy index
        int buddy_index = index ^ (size / MIN_BLOCK);
        
        // Validate buddy index
        if (buddy_index < 0 || buddy_index >= MAX_BLOCKS) {
            fprintf(stderr, "Error: Invalid buddy index.\n");
            return;  // No return value for void function
        }
        
        // Check if buddy can be merged
        if (memory[buddy_index]->is_free && memory[buddy_index]->size == size) {
            // Choose the lower index as the new block
            if (buddy_index < index) {
                index = buddy_index;
            }
            
            // Merge blocks
            memory[index]->size *= 2;
            
            // Clear the buddy
            memory[buddy_index]->size = 0;
            memory[buddy_index]->is_free = 0;
            memory[buddy_index]->pid = -1;
        } else {
            break;  // Can't merge anymore
        }
    }
}

void destruct_buddy() {
    if (memory == NULL) {
        return;  // Already deallocated
    }
    
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (memory[i] != NULL) {
            free(memory[i]);
            memory[i] = NULL;
        }
    }
    
    free(memory);
    memory = NULL;
    
    printf("Buddy system memory deallocated.\n");
    fflush(stdout);
}
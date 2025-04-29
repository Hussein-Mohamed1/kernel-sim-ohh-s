#include "buddy.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_BLOCKS (TOTAL_MEMORY / MIN_BLOCK)

Block** memory;

void init_buddy()
{
    printf("[BUDDY] Initializing buddy system...\n");

    FILE* log = fopen("memory.log", "w");
    fprintf(log, "#At\ttime\tx\tallocated\ty\tbytes\tfor\tprocess\tz\tfrom\ti\tto\tj");
    fclose(log);

    memory = (Block**)malloc(sizeof(Block*) * MAX_BLOCKS);
    if (!memory)
    {
        fprintf(stderr, "[BUDDY] Error: Memory allocation failed for memory array.\n");
        return;
    }

    // Initialize all memory blocks
    for (int i = 0; i < MAX_BLOCKS; i++)
    {
        memory[i] = (Block*)malloc(sizeof(Block));
        if (!memory[i])
        {
            // Clean up already allocated memory
            for (int j = 0; j < i; j++)
            {
                free(memory[j]);
            }
            free(memory);
            memory = NULL;
            fprintf(stderr, "[BUDDY] Error: Memory allocation failed for block %d.\n", i);
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
    printf("[BUDDY] Buddy system initialized. TOTAL_MEMORY=%d, MIN_BLOCK=%d, MAX_BLOCKS=%d\n", TOTAL_MEMORY, MIN_BLOCK,
           MAX_BLOCKS);
}

int allocate_memory(int pid, int size)
{
    printf("[BUDDY] Request to allocate %d bytes for pid %d\n", size, pid);
    if (size <= 0)
    {
        printf("[BUDDY] Invalid allocation size: %d\n", size);
        return -1; // Invalid size
    }

    // Calculate needed size (round up to nearest power of 2)
    int needed_size = MIN_BLOCK;
    while (needed_size < size)
    {
        needed_size *= 2;
        if (needed_size > TOTAL_MEMORY)
        {
            printf("[BUDDY] Requested size %d exceeds TOTAL_MEMORY %d\n", needed_size, TOTAL_MEMORY);
            return -1; // Requested size too large
        }
    }
    printf("[BUDDY] Needed block size: %d\n", needed_size);

    // Find a suitable block
    for (int i = 0; i < MAX_BLOCKS; i++)
    {
        if (memory[i]->is_free && memory[i]->size >= needed_size)
        {
            printf("[BUDDY] Found free block at index %d with size %d\n", i, memory[i]->size);
            // Split the block until it's the right size
            while (memory[i]->size > needed_size)
            {
                if (split_block(i) == 0)
                {
                    printf("[BUDDY] Failed to split block at index %d\n", i);
                    return -1; // Split failed
                }
                printf("[BUDDY] Split block at index %d, new size %d\n", i, memory[i]->size);
            }

            // Allocate the block
            memory[i]->is_free = 0;
            memory[i]->pid = pid;
            printf("[BUDDY] Allocated block at index %d (address %d) for pid %d\n", i, i * MIN_BLOCK, pid);
            return i * MIN_BLOCK; // Return the memory address
        }
    }

    printf("[BUDDY] No suitable block found for pid %d, size %d\n", pid, size);
    return -1; // No suitable block found
}

int split_block(int index)
{
    printf("[BUDDY] Splitting block at index %d\n", index);
    if (index < 0 || index >= MAX_BLOCKS || memory[index]->size == MIN_BLOCK)
    {
        printf("[BUDDY] Cannot split block at index %d (size: %d)\n", index, memory[index]->size);
        return 0; // Can't split this block
    }

    int half_size = memory[index]->size / 2;
    int buddy_index = find_free_index();

    if (buddy_index == -1)
    {
        printf("[BUDDY] No free index found for buddy block\n");
        return 0; // No free blocks available
    }

    // Calculate the buddy's position (should be at offset half_size from current block)
    int buddy_offset = index + (half_size / MIN_BLOCK);

    // If the buddy's position is already taken, use the free index we found
    if (buddy_offset < MAX_BLOCKS && memory[buddy_offset]->size == 0)
    {
        buddy_index = buddy_offset;
    }

    // Initialize the buddy block
    memory[buddy_index]->size = half_size;
    memory[buddy_index]->is_free = 1;
    memory[buddy_index]->pid = -1;
    printf("[BUDDY] Created buddy block at index %d with size %d\n", buddy_index, half_size);

    // Update the original block
    memory[index]->size = half_size;

    return 1; // Split successful
}

int find_free_index()
{
    for (int i = 0; i < MAX_BLOCKS; i++)
    {
        if (memory[i]->size == 0)
        {
            printf("[BUDDY] Found free index at %d\n", i);
            return i;
        }
    }
    printf("[BUDDY] No free index found\n");
    return -1; // No free blocks
}

void free_memory(int pid)
{
    printf("[BUDDY] Request to free memory for pid %d\n", pid);
    for (int i = 0; i < MAX_BLOCKS; i++)
    {
        if (memory[i]->pid == pid)
        {
            printf("[BUDDY] Found block at index %d for pid %d, freeing...\n", i, pid);
            memory[i]->is_free = 1;
            memory[i]->pid = -1;
            merge_buddies(i);
            return;
        }
    }
    printf("[BUDDY] No block found for pid %d\n", pid);
}

void merge_buddies(int index)
{
    printf("[BUDDY] Attempting to merge buddies for index %d\n", index);
    if (index < 0 || index >= MAX_BLOCKS)
    {
        printf("[BUDDY] Invalid index %d for merging\n", index);
        return; // Invalid index
    }

    while (1)
    {
        int size = memory[index]->size;
        if (size == TOTAL_MEMORY)
        {
            printf("[BUDDY] Block at index %d is already TOTAL_MEMORY, cannot merge further\n", index);
            break; // Can't merge anymore
        }

        // Calculate buddy index
        int buddy_index = index ^ (size / MIN_BLOCK);

        // Validate buddy index
        if (buddy_index < 0 || buddy_index >= MAX_BLOCKS)
        {
            fprintf(stderr, "[BUDDY] Error: Invalid buddy index %d for index %d\n", buddy_index, index);
            return; // No return value for void function
        }

        // Check if buddy can be merged
        if (memory[buddy_index]->is_free && memory[buddy_index]->size == size)
        {
            printf("[BUDDY] Merging block %d and buddy %d (size %d)\n", index, buddy_index, size);
            // Choose the lower index as the new block
            if (buddy_index < index)
            {
                index = buddy_index;
            }

            // Merge blocks
            memory[index]->size *= 2;

            // Clear the buddy
            memory[buddy_index]->size = 0;
            memory[buddy_index]->is_free = 0;
            memory[buddy_index]->pid = -1;
        }
        else
        {
            printf("[BUDDY] Cannot merge block %d with buddy %d\n", index, buddy_index);
            break; // Can't merge anymore
        }
    }
}

void destruct_buddy()
{
    printf("[BUDDY] Deallocating buddy system memory...\n");
    if (memory == NULL)
    {
        printf("[BUDDY] Memory already deallocated.\n");
        return; // Already deallocated
    }

    for (int i = 0; i < MAX_BLOCKS; i++)
    {
        if (memory[i] != NULL)
        {
            free(memory[i]);
            memory[i] = NULL;
        }
    }

    free(memory);
    memory = NULL;

    printf("[BUDDY] Buddy system memory deallocated.\n");
    fflush(stdout);
}

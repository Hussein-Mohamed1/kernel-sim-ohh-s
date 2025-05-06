#pragma once
#include <stdio.h>
#include <stdbool.h>
#include "../data_structures/min_heap.h"
#include "buddy.h"
#include "headers.h"
#include "clk.h"
// Memory Manager Structure
bool mm_init(unsigned int memory_size);
void mm_destroy();

// Memory Manager Functions
int mm_allocate(int pid, size_t size);
int mm_allocate_by_id(int process_id, size_t size); // New function for allocating by process ID
void mm_free(int pid);
void mm_free_by_id(int process_id);                 // New function for freeing by process ID

// Check if a PID has memory allocated
bool mm_check_pid_allocation(int pid, int* offset, size_t* size);
bool mm_check_id_allocation(int process_id, int* offset, size_t* size); // Check allocation by process ID

// Map process ID to PID
bool mm_map_id_to_pid(int process_id, int pid);     // Create mapping between process ID and PID
int mm_get_pid_by_id(int process_id);               // Get PID by process ID

// Waiting List Functions
int mm_add_to_waiting_list(processParameters* process_params);
processParameters* mm_get_next_allocatable_process();
bool mm_has_waiting_processes();
int mm_get_waiting_count();

// Memory logging functions
void mm_initialize_memory_log();
void mm_log_memory_allocation(int time, int id, int size, int start_address, int end_address);
void mm_log_memory_deallocation(int time, int id, int size, int start_address, int end_address);
void mm_close_memory_log();

// Debugging functions
void mm_dump_memory();
void mm_get_stats(size_t* total_memory, size_t* free_memory, size_t* largest_block);
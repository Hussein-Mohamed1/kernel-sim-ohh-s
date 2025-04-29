#pragma once

#include "pcb.h"
#include "min_heap.h"
#include "queue.h"

// Function prototypes
int compare_processes(const void* p1, const void* p2);
PCB* hpf(min_heap_t* ready_queue, int current_time);
PCB* srtn(min_heap_t* ready_queue);
PCB* rr(Queue* ready_queue, int current_time);
void log_process_state(PCB* process, char* state, int time);
void generate_statistics();
void log_memory_op(int time, int pid, int size, int start, int end, int is_alloc);
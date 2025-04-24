#ifndef SCHEDULER_UTILITIES_H
#define SCHEDULER_UTILITIES_H

#include "headers.h"

// Function prototypes
int compare_processes(const void* p1, const void* p2);
int init_scheduler();
PCB* hpf(min_heap_t* ready_queue, PCB* running_process, int current_time, int completed_process_count);
void log_process_state(PCB* process, char* state, int time);
void generate_statistics();


#endif
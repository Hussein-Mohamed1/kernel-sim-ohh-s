#include <stdio.h>

#include "headers.h"
#include "min_heap.h"
#include "pcb.h"

// Global variables
int process_count = 0;
PCB* running_process = NULL;
min_heap_t* ready_queue = NULL;
FILE* log_file = NULL;
finishedProcessInfo** finished_process_info;
int finished_processes_count;
int cpu_idle_time = 0;
int total_busy_time = 0;
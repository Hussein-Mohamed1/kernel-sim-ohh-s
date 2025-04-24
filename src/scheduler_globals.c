#include "headers.h"

// Global variables
int current_time = 0;
int process_count = 0;
int completed_process_count = 0;
PCB* running_process = NULL;
min_heap_t * ready_queue = NULL;
int msg_queue_id = -1;
FILE* log_file = NULL;
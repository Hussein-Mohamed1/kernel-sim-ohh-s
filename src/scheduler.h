#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "headers.h"
#include "pcb.h"
#include "queue.h"
#include "process.h"
#include "stats.h"

// Function prototypes for scheduler operations
void run_scheduler();
void initialize_scheduler(int alg, int quantum_val, FILE **log_file, int *msgq_id);
void schedule(int algorithm, int quantum, Node **ready_queue, PCB **running_process, 
             FILE *log_file, int current_time);
void cleanup(FILE *log_file, int msgq_id, Node **ready_queue, PCB **running_process);

#endif // SCHEDULER_H
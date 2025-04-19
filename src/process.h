#ifndef PROCESS_H
#define PROCESS_H

#include "pcb.h"
#include "queue.h"

// Function prototypes for process operations
void add_process(Node **ready_queue, PCB process, FILE *log_file, int current_time);
void start_process(PCB *process, PCB **running_process, FILE *log_file, int current_time);
void stop_process(PCB *process, Node **ready_queue, PCB **running_process, FILE *log_file, int current_time);
void handle_process_termination(int pid, Node *ready_queue, PCB **running_process, 
                               int *completed_processes, FILE *log_file, int current_time);
int check_process_termination(int msgq_id, Node *ready_queue, PCB **running_process, 
                             int *completed_processes, FILE *log_file, int current_time);
void log_process_state(PCB *process, char *state, FILE *log_file, int current_time);

#endif // PROCESS_H
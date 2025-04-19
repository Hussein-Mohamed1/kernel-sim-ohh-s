#ifndef QUEUE_H
#define QUEUE_H

#include "pcb.h"

// Function prototypes for queue operations
void add_to_ready_queue(Node **ready_queue, PCB process);
void remove_from_ready_queue(Node **ready_queue, PCB *process);
PCB* get_process_by_pid(Node *ready_queue, PCB *running_process, int pid);
PCB* get_next_process_hpf(Node **ready_queue);
PCB* get_next_process_srtn(Node **ready_queue);
PCB* get_next_process_rr(Node **ready_queue);
int queue_size(Node *ready_queue);

#endif // QUEUE_H
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "min_heap.h"
#include "headers.h"



// Message structure for IPC
typedef struct {
    long mtype;
    int process_id;
    int arrival_time;
    int runtime;
    int priority;
    int remaining_time;
} ProcessMessage;


void run_scheduler();
void sync_clk();
void generate_statistics();
int compare_processes(const void* a, const void* b);
void log_process_state(PCB* process, char* state, int time);

// Global variables declarations (extern)
extern int current_time;
extern int process_count;
extern int completed_process_count;
extern PCB* running_process;
extern min_heap_t* ready_queue;
extern int msg_queue_id;
extern FILE* log_file;
extern PCB** finished_processes;

#endif
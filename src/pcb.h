#ifndef PCB_H
#define PCB_H

#include "headers.h"

// Process Control Block (PCB)
typedef struct {
    int id;                 // Process ID
    int arrival_time;       // When the process arrives
    int burst_time;         // Total execution time needed
    int remaining_time;     // Remaining execution time
    int priority;           // Process priority (lower number = higher priority)
    int waiting_time;       // Total time spent waiting
    int start_time;         // When the process first starts executing
    int last_run_time;      // Last time process was running (for preemption)
    int completion_time;    // When the process completes
    int response_time;      // Time between arrival and first execution
    int turnaround_time;    // Time between arrival and completion
    float weighted_turnaround; // Turnaround time / Burst time
    int status;             // READY, RUNNING, TERMINATED
    int pid;                // Actual process ID from fork()
} PCB;

// Structure for ready queue node
typedef struct Node {
    PCB process;
    struct Node *next;
} Node;

#endif // PCB_H
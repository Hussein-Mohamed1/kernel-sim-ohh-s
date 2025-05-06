#pragma once

// Message structure for IPC
typedef struct
{
    long mtype;
    int id;
    int pid;
    int arrival_time;
    int runtime;
    int priority;
    int memsize;  // Added memory size for Phase 2
} processParameters;

typedef struct
{
    int ta; // process->finish_time - process->arrival_time;
    float wta; // (float)ta / process->runtime;
    int waiting_time;
} finishedProcessInfo;

#define MAX_INPUT_PROCESSES 100

// Extra States (IDK if we'll use them)
#define BLOCKED 2
#define PAUSED 3

// Scheduling algorithms
#define RR 0
#define HPF 1
#define SRTN 2

// Message types
#define PROCESS_ARRIVED 1
#define PROCESS_FINISHED 2

#define PROC_IDLE 0      // Process is waiting for instructions
#define PROC_RUNNING 1   // Process is running
#define PROC_FINISHED 2  // Process has finished its time slice

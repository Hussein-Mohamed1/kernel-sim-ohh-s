#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <math.h>
#include "clk.h"

// Constants
#define READY 0
#define RUNNING 1
#define TERMINATED 2

// Scheduling algorithms
#define HPF 1
#define SRTN 2
#define RR 3

// Message types
#define PROCESS_ARRIVED 1
#define PROCESS_FINISHED 2

// Message structure for process communication
typedef struct {
    long mtype;         // Message type (PROCESS_ARRIVED or PROCESS_FINISHED)
    int pid;            // Process ID (from fork)
    int id;             // Process ID (assigned by generator)
    int arrival_time;   // Time when process arrives
    int running_time;   // Total execution time needed
    int priority;       // Process priority
    int remaining_time; // Remaining time of the process
} ProcessMessage;

#endif // HEADERS_H
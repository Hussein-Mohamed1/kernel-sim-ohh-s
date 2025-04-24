#pragma once
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
#include "process.h"
#include "pcb.h"
#include "scheduler.h"
#include "data_structures/min_heap.h"
#include "scheduler_utils.h"
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


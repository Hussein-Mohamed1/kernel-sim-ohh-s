#pragma once

// Process states
#define PROC_IDLE 0      // Process is waiting for instructions
#define PROC_RUNNING 1   // Process is running
#define PROC_FINISHED 2  // Process has finished its time slice
#include <signal.h>

void sigIntHandler(int signum);
void sigStpHandler(int signum);
void sigContHandler(int signum);
void run_process(int runtime, pid_t process_generator_pid);
void update_process_state(int proc_shmid, int state);

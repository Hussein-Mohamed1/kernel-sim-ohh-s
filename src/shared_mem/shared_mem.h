#pragma once

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 400
#define MAX_PROCESSES 100

// Process control block for shared memory
typedef struct {
    int pid;            // Process ID
    int time_slice;     // Time slice allocated to the process
    int state;          // Process state (PROC_IDLE, PROC_RUNNING, PROC_FINISHED)
} process_control_t;

// Shared memory functions
int create_shared_memory(key_t key);
void cleanup_shared_memory(int shm_id);
void write_process_control(int shm_id, int pid, int time_slice, int status);
process_control_t read_process_control(int shm_id, int pid);

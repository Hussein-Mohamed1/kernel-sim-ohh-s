#include "shared_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include "colors.h"
#include "headers.h"

// Debug flag
#ifndef DEBUG
#define DEBUG 0
#endif

int create_shared_memory(const key_t key)
{
    int shmid = shmget(key, sizeof(process_control_t), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("Failed to create shared memory");
        return -1;
    }

    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[SHARED_MEM] Shared memory created with ID: %d\n"ANSI_COLOR_RESET, shmid);
    return shmid;
}

void cleanup_shared_memory(int shm_id)
{
    if (shm_id != -1)
    {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1)
        {
            perror("Failed to remove shared memory");
        }
        else if (DEBUG)
        {
            printf(ANSI_COLOR_BLUE"[SHARED_MEM] Shared memory segment %d removed\n"ANSI_COLOR_RESET, shm_id);
        }
    }
}

void write_process_control(int shm_id, int pid, int time_slice, int status)
{
    if (shm_id == -1) return;

    process_control_t* ctrl = (process_control_t*)shmat(shm_id, NULL, 0);
    if ((void*)ctrl == (void*)-1)
    {
        perror("Error attaching shared memory in write_process_control");
        return;
    }

    // Find the control block for the specific process
    ctrl->pid = pid;
    ctrl->time_slice = time_slice;
    ctrl->state = status;

    if (DEBUG)
        printf(
            ANSI_COLOR_BLUE"[SHARED_MEM] Updated control for PID %d: time_slice=%d, state=%d\n"ANSI_COLOR_RESET,
            pid, time_slice, status);

    shmdt(ctrl);
}

process_control_t read_process_control(int shm_id, int pid)
{
    process_control_t ctrl = {0, 0, 0}; // Initialize with zeros

    if (shm_id == -1) return ctrl;

    process_control_t* shm = (process_control_t*)shmat(shm_id, NULL, 0);
    if ((void*)shm == (void*)-1)
    {
        perror("Error attaching shared memory in read_process_control");
        return ctrl;
    }

    // Check if this control block matches the requested pid
    if (shm->pid == pid)
    {
        ctrl = *shm;
    }

    shmdt(shm);
    return ctrl;
}

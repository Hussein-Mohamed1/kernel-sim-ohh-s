#include "process.h"
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/shm.h>
#include "clk.h"
#include "colors.h"
#include "../shared_mem/shared_mem.h"
int proc_shmid = -1;

// Simplified process implementation
void run_process(const int runtime, const pid_t process_generator_pid)
{
    // Get shared memory ID
    proc_shmid = shmget(SHM_KEY, sizeof(process_control_t), 0666);
    if (proc_shmid == -1)
    {
        if (DEBUG)
            perror("[PROCESS] Error getting shared memory");
        return;
    }

    // Sync clock
    sync_clk();

    int remaining = runtime;

    while (remaining > 0)
    {
        // After receiving SIGCONT, read process control
        process_control_t ctrl = read_process_control(proc_shmid, getpid());

        while (ctrl.pid != getpid() || ctrl.state != PROC_RUNNING)
        {
            // re-read the control block when waking up
            ctrl = read_process_control(proc_shmid, getpid());
        }

        // Determine how long to run
        int time_to_run = (ctrl.time_slice < remaining) ? ctrl.time_slice : remaining;

        if (DEBUG)
            printf("[PROCESS] Process %d running for %d time units\n", getpid(), time_to_run);

        // Simulate execution
        int start_time = get_clk();
        int elapsed = 0;

        while (elapsed < time_to_run)
        {
            int now = get_clk();
            if (now > start_time)
            {
                elapsed += (now - start_time);
                start_time = now;
            }
            // usleep(100); // Short sleep to reduce CPU load
        }

        // Update remaining time
        remaining -= time_to_run;

        // Signal completion of time slice
        update_process_state(proc_shmid, (remaining > 0) ? PROC_IDLE : PROC_FINISHED);

        if (DEBUG)
            printf("[PROCESS] Process %d finished slice, remaining: %d\n", getpid(), remaining);
    }

    // Process is done, notify scheduler via SIGCHLD
    kill(process_generator_pid, SIGCHLD);
}

void sigIntHandler(int signum)
{
    // This handler is still needed for clean termination on SIGINT
    printf(ANSI_COLOR_YELLOW "[PROCESS] Process %d received SIGINT. Terminating...\n"ANSI_COLOR_WHITE, getpid());
    destroy_clk(0);
    exit(0);
}

void sigStpHandler(int signum)
{
    // Update process state rather than status
    update_process_state(proc_shmid, PROC_IDLE);

    if (DEBUG)
        printf(ANSI_COLOR_YELLOW "[PROCESS] Process %d stopped.\n"ANSI_COLOR_WHITE, getpid());

    pause();
    signal(SIGTSTP, sigStpHandler);
}

void sigContHandler(int signum)
{
    if (DEBUG)
        printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d received SIGCONT. Resuming...\n"ANSI_COLOR_WHITE, getpid());

    signal(SIGCONT, sigContHandler);
}

void update_process_state(int proc_shmid, int state)
{
    if (proc_shmid == -1) return;

    process_control_t* ctrl = (process_control_t*)shmat(proc_shmid, NULL, 0);
    if ((void*)ctrl == (void*)-1) return;

    if (ctrl->pid == getpid())
    {
        ctrl->state = state;
    }

    shmdt(ctrl);
}

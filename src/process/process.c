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
#include <sched.h>
// Global variables for tracking process state
int proc_shmid = -1;
int remaining_runtime = 0;
int last_start_time = 0;
int is_running = 0;

// Simplified process implementation
void run_process(const int runtime, const pid_t process_generator_pid)
{
    signal(SIGTSTP, sigStpHandler);
    signal(SIGINT, sigIntHandler);
    signal(SIGCONT, sigContHandler);

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

    // Initialize runtime tracking
    remaining_runtime = runtime;

    // Main busy wait loop - just wait until we're done
    while (remaining_runtime > 0)
    {
        // Wait for SIGCONT to start/resume execution
        while (!is_running && remaining_runtime > 0)
        {
            // Idle wait
            usleep(1000); // Small sleep to avoid CPU hogging
        }

        // Process is now running
        if (remaining_runtime <= 0) break;

        // Read shared memory to get current control info
        process_control_t ctrl = read_process_control(proc_shmid, getpid());

        if (ctrl.state == PROC_RUNNING)
        {
            // Block signals during time update to make it atomic
            sigset_t block_set, old_set;
            sigemptyset(&block_set);
            sigaddset(&block_set, SIGTSTP);
            sigprocmask(SIG_BLOCK, &block_set, &old_set);

            // Update time tracking
            int current_time = get_clk();
            if (current_time > last_start_time)
            {
                int elapsed = current_time - last_start_time;
                remaining_runtime -= elapsed;
                last_start_time = current_time;

                if (DEBUG)
                    printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d running, remaining: %d\n"ANSI_COLOR_RESET,
                           getpid(), remaining_runtime);
            }

            // Restore original signal mask
            sigprocmask(SIG_SETMASK, &old_set, NULL);

            // Allow any pending signals to be delivered before continuing
            sched_yield();

            // Check if we're still running (might have changed due to signal)
            if (!is_running) continue;

            // Check if we've completed execution
            if (remaining_runtime <= 0)
            {
                update_process_state(proc_shmid, PROC_FINISHED);
                break;
            }
        }
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
    // Update the remaining runtime based on elapsed time
    int current_time = get_clk();
    int elapsed = current_time - last_start_time;
    remaining_runtime -= elapsed;

    // Update process state
    update_process_state(proc_shmid, PROC_IDLE);
    is_running = 0;

    if (DEBUG)
        printf(ANSI_COLOR_YELLOW "[PROCESS] Process %d stopped. Remaining runtime: %d\n"ANSI_COLOR_WHITE,
               getpid(), remaining_runtime);

    signal(SIGTSTP, sigStpHandler);
    pause();
}

void sigContHandler(int signum)
{
    // Update start time for runtime tracking
    last_start_time = get_clk();
    is_running = 1;

    if (DEBUG)
        printf(
            ANSI_COLOR_YELLOW"[PROCESS] Process %d received SIGCONT. Resuming with %d time units left\n"
            ANSI_COLOR_WHITE,
            getpid(), remaining_runtime);

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

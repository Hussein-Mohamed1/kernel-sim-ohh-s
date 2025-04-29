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
#include "shared_mem.h"

pid_t process_generator_pid;
int proc_shmid = -1;


void run_process(int runtime)
{
    // Get shared memory ID
    proc_shmid = shmget(SHM_KEY, sizeof(process_info_t), 0666);
    if (proc_shmid == -1)
    {
        if (DEBUG)
            perror("[PROCESS] Error getting shared memory");
        proc_shmid = -1;
    }

    // Sync clock before any get_clk() usage!
    sync_clk();

    // Make the process wait when spawned until it is told to run
    while (!get_process_status(proc_shmid))
        usleep(1);

    if (DEBUG)
        printf(ANSI_COLOR_YELLOW"[PROCESS] %d Woke Up For The First Time\n"ANSI_COLOR_WHITE, getpid());
    if (DEBUG)
        printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d started with runtime %d seconds.\n"ANSI_COLOR_WHITE, getpid(),
               runtime);
    int remaining = runtime;
    while (remaining > 0)
    {
        // Continuously check status from shared memory
        // If status is 1 and current_clk matches, run the process
        if (get_process_status(proc_shmid) && get_process_info(proc_shmid).current_clk == get_clk())
        {
            int time_to_run = 0;
            while (time_to_run <= 0)
            {
                time_to_run = get_time_to_run(proc_shmid, getpid());
                usleep(1000);
            }
            if (time_to_run > remaining)
                time_to_run = remaining;

            printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d will run for %d units (remaining: %d)\n"ANSI_COLOR_WHITE,
                   getpid(), time_to_run, remaining);

            int start_time = get_clk();
            int elapsed = 0;

            // At each tick, check handshake and status
            while (elapsed < time_to_run)
            {
                int now = get_clk();
                if (now != start_time)
                {
                    if (DEBUG)
                        printf(
                            ANSI_COLOR_YELLOW
                            "[PROCESS] PID %d ran for 1 second. Remaining: %d, Remaining in slice: %d\n"
                            ANSI_COLOR_WHITE,
                            getpid(), remaining - elapsed - 1, time_to_run - elapsed - 1);
                    elapsed++;
                    start_time = now;
                }
                usleep(1);
            }

            // Update remaining time
            remaining -= time_to_run;
            update_process_status(proc_shmid, getpid(), 0);
            if (DEBUG)
                printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d finished time slice, remaining: %d\n"ANSI_COLOR_WHITE,
                       getpid(), remaining);
        }
        // else
        // {
        //     // // Update status in shared memory
        //     // update_process_status(proc_shmid, getpid(), 0);
        //     raise(SIGTSTP); // Signal process to stop itself
        // }
    }

    // Finished execution
    update_process_status(proc_shmid, getpid(), 0);
    kill(process_generator_pid, SIGCHLD);
    if (DEBUG)
        printf(ANSI_COLOR_YELLOW"[PROCESS] Sending SIGCHLD to: %d\n"ANSI_COLOR_WHITE, process_generator_pid);
    printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d finished execution.\n"ANSI_COLOR_WHITE, getpid());
    destroy_clk(0);
}

int main(int argc, char* argv[])
{
    signal(SIGINT, sigIntHandler);
    signal(SIGCONT, sigContHandler);
    signal(SIGTSTP, sigStpHandler);

    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <runtime> <process_generator_pid>\n", argv[0]);
        return 1;
    }

    int runtime = atoi(argv[1]);
    process_generator_pid = atoi(argv[2]);

    if (runtime < 0 || process_generator_pid < 0)
    {
        if (runtime < 0)
            fprintf(stderr, "Runtime must be a positive integer.\n");
        else
            fprintf(stderr, "process_generator_pid must be a positive integer.\n");
        return 1;
    }

    run_process(runtime);
    return 0;
}


void sigIntHandler(int signum)
{
    printf(ANSI_COLOR_YELLOW "[PROCESS] Process %d received SIGINT. Terminating...\n"ANSI_COLOR_WHITE, getpid());
    destroy_clk(0);
    exit(0);
}

void sigStpHandler(int signum)
{
    // Update status in shared memory to not running
    update_process_status(proc_shmid, getpid(), 0);
    if (DEBUG)
        printf(ANSI_COLOR_YELLOW "[PROCESS] Process %d stopped.\n"ANSI_COLOR_WHITE, getpid());

    pause();
    signal(SIGTSTP, sigStpHandler);
}

void sigContHandler(int signum)
{
    // Uncomment this for more explicit resume logging
    if (DEBUG)
        printf(ANSI_COLOR_YELLOW"[PROCESS] Process %d received SIGCONT. Resuming...\n"ANSI_COLOR_WHITE, getpid());

    // Update status in shared memory to running
    update_process_status(proc_shmid, getpid(), 1);

    signal(SIGCONT, sigContHandler);
}

process_info_t get_process_info(int proc_shmid)
{
    process_info_t process_info = {.status = -1, .pid = -1, .time_to_run = -1, .current_clk = -1};

    if (proc_shmid == -1) return process_info;
    process_info_t* shm = (process_info_t*)shmat(proc_shmid, NULL, 0);
    if ((void*)shm == (void*)-1)
    {
        if (DEBUG)
            perror("Error attaching shared memory in process");
        return process_info;
    }

    if (shm->pid == getpid())
    {
        process_info.time_to_run = shm->time_to_run;
        process_info.status = shm->status;
        process_info.pid = shm->pid;
        process_info.current_clk = shm->current_clk;
        shmdt(shm);
        return process_info;
    }

    shmdt(shm);
    return process_info;
}

int get_time_to_run(int proc_shmid, pid_t pid)
{
    if (proc_shmid == -1) return -1;
    process_info_t* shm = (process_info_t*)shmat(proc_shmid, NULL, 0);
    if ((void*)shm == (void*)-1)
    {
        if (DEBUG)

            perror("Error attaching shared memory in process");
        return -1;
    }
    int remaining_time = -1;
    int status = 0;
    if (shm->pid == pid && shm->status == 1 && shm->current_clk == get_clk())
    {
        remaining_time = shm->time_to_run;
        status = shm->status;
    }
    else
    {
        // Process is not running, return -1
        remaining_time = -1;
    }

    shmdt(shm);
    return status ? remaining_time : -1;
}

int get_process_status(int proc_shmid)
{
    if (proc_shmid == -1) return 0;
    process_info_t* shm = (process_info_t*)shmat(proc_shmid, NULL, 0);
    if ((void*)shm == (void*)-1)
    {
        if (DEBUG)

            perror("Error attaching shared memory in process");
        return 0;
    }

    int status = 0;
    if (shm->pid == getpid() && shm->current_clk == get_clk())
    {
        status = shm->status;
    }

    shmdt(shm);
    return status;
}

void update_process_status(int proc_shmid, pid_t pid, int status)
{
    if (proc_shmid == -1) return;
    process_info_t* shm = (process_info_t*)shmat(proc_shmid, NULL, 0);
    if ((void*)shm == (void*)-1) return;

    if (shm->pid == pid)
        shm->status = status;

    shmdt(shm);
}

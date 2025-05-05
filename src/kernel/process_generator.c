#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>    // for fork, execl
#include <sys/ipc.h>
#include <sys/types.h> // for pid_t
#include <sys/msg.h>
#include "clk.h"
#include "headers.h"
#include "process_generator.h"
#include <string.h>
#include <sys/wait.h>
#include "colors.h"

#include "scheduler.h"
#include <bits/getopt_core.h>

#include "shared_mem.h"
#include "../process/process.h"
int scheduler_type = -1; // Default invalid value
char* process_file = "processes.txt"; // Default filename
int quantum = 2; // Default quantum value
processParameters** process_parameters;
int msgid;
key_t key;

int main(int argc, char* argv[])
{
    int process_count;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:f:q:")) != -1)
    {
        switch (opt)
        {
        case 's':
            if (strcmp(optarg, "rr") == 0)
            {
                scheduler_type = 0; // RR
            }
            else if (strcmp(optarg, "hpf") == 0)
            {
                scheduler_type = 1; // HPF
            }
            else if (strcmp(optarg, "srtn") == 0)
            {
                scheduler_type = 2; // SRTN
            }
            else
            {
                fprintf(stderr, "Invalid scheduler type: %s\n", optarg);
                fprintf(stderr, "Valid options are: rr, hpf, srtn\n");
                exit(EXIT_FAILURE);
            }
            printf(ANSI_COLOR_MAGENTA"[MAIN] Using scheduler: %s\n"ANSI_COLOR_RESET, optarg);
            break;
        case 'f':
            process_file = optarg;
            printf(ANSI_COLOR_MAGENTA"[MAIN] Reading processes from: %s\n"ANSI_COLOR_RESET, process_file);
            break;
        case 'q':
            quantum = atoi(optarg);
            printf(ANSI_COLOR_MAGENTA"[MAIN] Quantum set to: %d\n"ANSI_COLOR_RESET, quantum);
            break;
        default:
            fprintf(stderr, "Usage: %s -s <scheduling-algorithm> -f <processes-text-file> [-q <quantum>]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Check if scheduler type is provided
    if (scheduler_type == -1)
    {
        fprintf(stderr, "Scheduler type must be specified with -s option\n");
        fprintf(stderr, "Usage: %s -s <scheduling-algorithm> -f <processes-text-file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get List of processes
    process_parameters = read_process_file(process_file, &process_count);

    // Init IPC
    // Any file name
    key = ftok("process_generator", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1)
    {
        perror("Error creating message queue");
        exit(1);
    }

    pid_t process_generator_pid = getpid();
    pid_t clk_pid = fork();
    // Child b
    // Child -> Fork processes and sends their pcb to the scheduler at the appropriate time
    if (clk_pid == 0)
    {
        // Child -> run_scheduler
        pid_t scheduler_pid = fork();
        if (scheduler_pid == 0)
        {
            signal(SIGINT, process_generator_cleanup);
            signal(SIGCHLD, child_process_handler);
            sync_clk();

            int remaining_processes = process_count;
            int crt_clk = get_clk();
            int old_clk = crt_clk;
            while (remaining_processes > 0)
            {
                while ((crt_clk = get_clk()) == old_clk);
                old_clk = crt_clk;

                int messages_sent = 0;
                // Check the process_parameters[] for processes whose arrival time == crt_clk, and fork/send them
                for (int i = 0; i < process_count; i++)
                {
                    if (process_parameters[i] != NULL && process_parameters[i]->arrival_time == crt_clk)
                    {
                        // Fork the process at its arrival time
                        const pid_t process_pid = fork();
                        if (process_pid == 0)
                        {
                            // In child: run_process directly
                            run_process(process_parameters[i]->runtime, process_generator_pid);
                            exit(0);
                        }
                        else if (process_pid > 0)
                        {
                            process_parameters[i]->pid = process_pid;
                            // Send PCB to scheduler
                            PCB proc_pcb = {
                                1, process_parameters[i]->id, process_parameters[i]->pid,
                                process_parameters[i]->arrival_time, process_parameters[i]->runtime,
                                process_parameters[i]->runtime, process_parameters[i]->priority, 0, -1, -1, -1, -1, -1,
                                -1,
                                PROC_IDLE,
                                process_parameters[i]->memsize, -1
                            };
                            if (msgsnd(msgid, &proc_pcb, sizeof(PCB) - sizeof(long), 0) == -1)
                            {
                                if (DEBUG)
                                    perror("Error sending message");
                            }
                        }
                        else
                        {
                            perror("fork failed");
                        }

                        // Cleanup
                        free(process_parameters[i]);
                        process_parameters[i] = NULL;
                        remaining_processes--;
                        messages_sent++;
                    }
                    else if (process_parameters[i] != NULL && process_parameters[i]->arrival_time > crt_clk)
                        break;
                }

                if (messages_sent > 0 && DEBUG)
                    printf(ANSI_COLOR_MAGENTA"[MAIN] Sent %d message(s) to scheduler\n"ANSI_COLOR_RESET, messages_sent);
            }

            if (DEBUG)
                printf(
                    ANSI_COLOR_MAGENTA"[MAIN] All processes have been sent, waiting until all processes terminate\n"
                    ANSI_COLOR_RESET);

            // Wait for all children to exit before cleanup
            int status;
            pid_t pid;
            while ((pid = waitpid(-1, &status, 0)) > 0) {
                if (DEBUG)
                    printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Child process %d exited with status %d\n"ANSI_COLOR_RESET,
                           pid, WEXITSTATUS(status));
            }

            process_generator_cleanup(0);
            exit(0);
        }
        else
        {
            // Parent
            init_clk();
            sync_clk();
            run_clk();
        }
    }
    else
    {
        if (DEBUG)
            printf(ANSI_COLOR_MAGENTA"[MAIN] Running Scheduler with pid: %d\n"ANSI_COLOR_RESET, getpid());
        run_scheduler();
    }

    return 0;
}

/*
 * Reads the input file and returns a ProcessMessage**, a pointer to an
 * array of ProcessMessage, of size MAX_INPUT_PROCESSES
 */
processParameters** read_process_file(const char* filename, int* count)
{
    FILE* file = fopen(filename, "r");

    if (!file)
    {
        perror(ANSI_COLOR_MAGENTA"[MAIN] Error opening file"ANSI_COLOR_RESET);
        exit(1);
    }

    // Count lines to allocate memory
    char line[256];
    int line_count = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Skip comment lines that start with #
        if (line[0] != '#' && line[0] != '\n')
        {
            line_count++;
        }
    }

    // Allocate memory for process message pointers
    /// Assuming a maximum of MAX_INPUT_PROCESSES processes
    processParameters** process_messages = (processParameters**)
        malloc(MAX_INPUT_PROCESSES * sizeof(processParameters*));

    // Initialize all pointers to NULL
    for (int i = 0; i < MAX_INPUT_PROCESSES; i++)
    {
        process_messages[i] = NULL;
    }

    *count = line_count;

    // Reset file pointer to beginning
    rewind(file);

    // Read process data
    int index = 0;

    while (fgets(line, sizeof(line), file))
    {
        // Skip comment lines that start with #
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }

        // Parse process information
        int id, arrival, runtime, priority, memsize;
        if (sscanf(line, "%d\t%d\t%d\t%d\t%d", &id, &arrival, &runtime, &priority, &memsize) == 5)
        {
            // Allocate memory for each ProcessMessage
            process_messages[index] = (processParameters*)malloc(sizeof(processParameters));

            // Set values
            process_messages[index]->mtype = 1; // Default message type
            process_messages[index]->id = id;
            process_messages[index]->arrival_time = arrival;
            process_messages[index]->runtime = runtime;
            process_messages[index]->priority = priority;
            process_messages[index]->memsize = memsize;
            index++;
        }
    }

    fclose(file);

    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Read %d processes from file\n"ANSI_COLOR_RESET, index);

    return process_messages;
}

void child_process_handler(int signum)
{
    int status;
    pid_t pid;

    // Reap all terminated children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (DEBUG)
            printf(
                ANSI_COLOR_BLUE"[PROC_GENERATOR] Acknowledged that child process PID: %d has died.\n"ANSI_COLOR_RESET,
                pid);
    }
}

void process_generator_cleanup(int signum)
{
    // Add static flag to prevent double execution
    static int cleanup_in_progress = 0;

    // Guard against recursive calls
    if (cleanup_in_progress) return;
    cleanup_in_progress = 1;

    // Free each ProcessMessage
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_parameters[i] != NULL)
        {
            free(process_parameters[i]);
            process_parameters[i] = NULL;
        }
    }

    if (process_parameters != NULL)
    {
        free(process_parameters);
        process_parameters = NULL; // Ensure this is set to NULL
    }

    // Wait until message queue is empty before removing it
    if (msgid != -1)
    {
        struct msqid_ds queue_info;

        // Check if there are messages in the queue
        while (1)
        {
            if (msgctl(msgid, IPC_STAT, &queue_info) == -1)
            {
                if (DEBUG)
                    perror("Error getting message queue stats");
                break;
            }

            // If no messages are left in the queue, we can safely remove it
            if (queue_info.msg_qnum == 0)
            {
                if (DEBUG)
                    printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Message queue is empty, removing it\n"ANSI_COLOR_RESET);
                break;
            }

            if (DEBUG)
                printf(
                    ANSI_COLOR_BLUE"[PROC_GENERATOR] Waiting for queue to empty: %ld messages remaining\n"
                    ANSI_COLOR_RESET,
                    queue_info.msg_qnum);
            sleep(1);
        }

        // remove the message queue if still exists
        struct msqid_ds queue;
        if (msgctl(msgid, IPC_STAT, &queue) != -1)
        {
            msgctl(msgid, IPC_RMID, NULL);
            msgid = -1;
            if (DEBUG)
                printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Message queue removed successfully\n"ANSI_COLOR_RESET);
        }
    }

    // Wait for all child processes to exit
    int status;
    pid_t pid;

    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Waiting for all child processes to exit\n"ANSI_COLOR_RESET);

    // Wait for any remaining children (blocking wait)
    while ((pid = waitpid(-1, &status, 0)) > 0)
    {
        if (DEBUG)
            printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Child process %d exited with status %d\n"ANSI_COLOR_RESET,
                   pid, WEXITSTATUS(status));
    }

    if (DEBUG && pid == -1)
        printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] All child processes have exited\n"ANSI_COLOR_RESET);

    destroy_clk(1);
    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Resources cleaned up\n"ANSI_COLOR_RESET);


    exit(0);
}

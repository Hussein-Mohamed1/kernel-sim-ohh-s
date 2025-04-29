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

    /*
     * Fork -> sends the processes at the appropriate time to the scheduler
     * Parent -> runs the clk
     */
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
            int old_clk = -1;
            while (remaining_processes > 0)
            {
                // 0 1 2 3 4
                // Ensure that we move by increments of 1
                while ((crt_clk = get_clk()) - old_clk == 0)
                    usleep(1); // 1us sleep;
                old_clk = crt_clk;

                int messages_sent = 0;
                // Check the process_parameters[] for processes whose arrival time == crt_clk, and fork/send them
                for (int i = 0; i < process_count; i++)
                {
                    if (process_parameters[i] != NULL && process_parameters[i]->arrival_time == crt_clk)
                    {
                        // Fork the process at its arrival time
                        pid_t pid = fork();
                        if (pid == 0)
                        {
                            char runtime_str[16];
                            snprintf(runtime_str, sizeof(runtime_str), "%d", process_parameters[i]->runtime);
                            char pid_str[16];
                            snprintf(pid_str, sizeof(pid_str), "%d", process_generator_pid);
                            // printf("[CHILD A] Sending Pid: %d\n", process_generator_pid);
                            execl("./process", "process", runtime_str, pid_str, (char*)NULL);
                            perror("execl failed");
                            exit(1);
                        }
                        else if (pid > 0)
                        {
                            process_parameters[i]->pid = pid;
                            // kill(pid, SIGTSTP); // Immediately stop the child process
                        }
                        else
                        {
                            perror("fork failed");
                        }

                        messages_sent++;
                        PCB proc_pcb = {
                            1, process_parameters[i]->id, process_parameters[i]->pid,
                            process_parameters[i]->arrival_time, process_parameters[i]->runtime,
                            process_parameters[i]->runtime, process_parameters[i]->priority, 0, -1, -1, -1, -1, -1,
                            -1,
                            READY,
                            process_parameters[i]->memsize, -1
                        };
                        // Send the message
                        if (msgsnd(msgid, &proc_pcb, sizeof(PCB), 0) == -1)
                        {
                            if (DEBUG)
                                perror("Error sending message");
                        }

                        // Cleanup
                        free(process_parameters[i]);
                        process_parameters[i] = NULL;
                        remaining_processes--;
                    }
                    else if (process_parameters[i] != NULL && process_parameters[i]->arrival_time > crt_clk)
                        break;
                }

                if (messages_sent > 0 && DEBUG)
                    printf(ANSI_COLOR_MAGENTA"[MAIN] Sent %d message(s) to scheduler\n"ANSI_COLOR_RESET, messages_sent);
            }

            if (DEBUG)
                printf(ANSI_COLOR_MAGENTA"[MAIN] All processes have been sent, exiting...\n"ANSI_COLOR_RESET);
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
    signal(signum, process_generator_cleanup);

    // Free each ProcessMessage
    for (int i = 0; i < process_count; i++)
    {
        if (process_parameters[i] != NULL)
        {
            free(process_parameters[i]);
            process_parameters[i] = NULL;
        }
    }

    if (process_parameters != NULL)
        free(process_parameters);

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
            usleep(100000); // Sleep for 100ms before checking again
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

    destroy_clk(0);
    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[PROC_GENERATOR] Resources cleaned up\n"ANSI_COLOR_RESET);

    if (signum != 0)
    {
        exit(signum);
    }
}

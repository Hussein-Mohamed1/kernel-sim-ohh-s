#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>    // for fork, execl
#include <sys/ipc.h>
#include <sys/types.h> // for pid_t
#include <sys/msg.h>
#include "clk.h"
#include "headers.h"
#include "scheduler.h"

#define RUN_TILL 5
#define MAX_INPUT_PROCESSES 100

void clear_resources(int);
ProcessMessage** read_process_file(const char* filename, int* count);

extern process_count;
ProcessMessage** process_messages;
int msgid;

int main(int argc, char* argv[])
{
    // Get List of processes
    process_messages = read_process_file("processes.txt", &process_count);

    // Init IPC
    // Any file name
    key_t key = ftok("process_generator", 65);
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
    pid_t clk_pid = fork();

    if (clk_pid == 0)
    {
        signal(SIGINT, clear_resources);
        sync_clk();

        int remaining_processes = process_count;
        int crt_clk = get_clk();
        int old_clk = 0;
        while (remaining_processes > 0)
        {
            // Ensure that we move by increments of 1
            while ((crt_clk = get_clk()) - old_clk == 0)
                usleep(1000); // 1ms sleep;
            old_clk = crt_clk;

            printf("current time is %d\n", crt_clk);

            int messages_sent = 0;
            // Check the ProcessMessages[] for processes whose arrival time == crt_clk, and send them
            for (int i = 0; i < process_count; i++)
            {
                if (process_messages[i] != NULL && process_messages[i]->arrival_time == crt_clk)
                {
                    messages_sent++;
                    // Send the message
                    if (msgsnd(msgid, process_messages[i], sizeof(ProcessMessage) - sizeof(long), 0) == -1)
                    {
                        perror("Error sending message");
                    }

                    // Cleanup
                    free(process_messages[i]);
                    process_messages[i] = NULL;
                    remaining_processes--;
                }
            }

            if (messages_sent > 0)
            {
                printf("Sent %d message(s) to scheduler\n", messages_sent);
            }
        }

        printf("All processes have been sent, exiting...\n");
        clear_resources(0);
        destroy_clk(1);
        exit(0);
    }
    else
    {
        init_clk();
        sync_clk();
        run_clk();
    }

    return 0;
}

/*
 * Reads the input file and returns a ProcessMessage**, a pointer to an
 * array of ProcessMessage, of size MAX_INPUT_PROCESSES
 */
ProcessMessage** read_process_file(const char* filename, int* count)
{
    FILE* file = fopen(filename, "r");

    if (!file)
    {
        perror("Error opening file");
        exit(1);
    }

    // Count lines to allocate memory
    char line[100];
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
    ProcessMessage** process_messages = (ProcessMessage**)malloc(MAX_INPUT_PROCESSES * sizeof(ProcessMessage*));

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
        int id, arrival, runtime, priority;
        if (sscanf(line, "%d\t%d\t%d\t%d", &id, &arrival, &runtime, &priority) == 4)
        {
            // Allocate memory for each ProcessMessage
            process_messages[index] = (ProcessMessage*)malloc(sizeof(ProcessMessage));

            // Set values
            process_messages[index]->mtype = 1; // Default message type
            process_messages[index]->process_id = id;
            process_messages[index]->arrival_time = arrival;
            process_messages[index]->runtime = runtime;
            process_messages[index]->priority = priority;

            index++;
        }
    }

    fclose(file);

    printf("Read %d processes from file\n", index);

    return process_messages;
}

void clear_resources(int signum)
{
    // Free each ProcessMessage
    for (int i = 0; i < process_count; i++)
    {
        if (process_messages[i] != NULL)
        {
            free(process_messages[i]);
            process_messages[i] = NULL;
        }
    }

    // Free the array of pointers
    free(process_messages);

    // Remove message queue
    if (msgid != -1)
    {
        msgctl(msgid, IPC_RMID, NULL);
    }

    printf("Resources cleaned up\n");

    if (signum != 0)
    {
        exit(signum);
    }
}

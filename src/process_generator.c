#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "clk.h"
#include "src/pcb.h"
#include "data_structures/headers.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include "queue.h"
#include "clk.h"

Queue* processes = NULL;


Queue* readPorcessFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if(!file) {
        perror("Invalid file name");
        exit(0);
    }

    Queue* processes = malloc(sizeof(Queue));
    initQueue(processes, sizeof(PCB));

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if(line[0] == '#' || line[0] == '\n') continue;

        PCB process = {0};
        if(sscanf(line, "%d %d %d %d", &process.id, &process.arrival_time, &process.burst_time, &process.priority) < 4) {
            fprintf("Invalid process format: %s", line);
            continue;
        }

        process.remaining_time = process.burst_time;
        
        //TODO: initialize the remaining data members after scheduler implementation

        enqueue(processes, &process);
        printf("Loaded process: ID=%d, AT=%d, BT=%d, Priority=%d\n",
            process.id, process.arrival_time, 
            process.burst_time, process.priority);
        }

        fclose(file);
        return processes;
    }


void clear_resources(int signum) {

    // TODO: Clean up scheduler resources and anything else
    
    destroy_clk(1);
    printf("Process generator terminated cleanly\n");
    exit(0);
}

void createSchedulerProcess(/* To be determined */) {
    // TODO: fork the scheduler
    
    // scheduler_pid = fork();

    // if (scheduler_pid == 0) {

    // }
    
}

void sendProcessToScheduler(/* To be determined */) {
    /*
        TODOS
        1- Message queue initialization
        2- cmp getclk() with next_process.arrivalTime
        3- if matches -> consume
    */
}



int main(int argc, char * argv[])
{
    if(argc < 2) {
        printf("Usage: %s <process_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, clear_resources);
    signal(SIGTERM, clear_resources);

    init_clk();    
    pid_t clk_pid = fork();
    if (clk_pid == 0)  {
        run_clk();
        exit(0);
    }

    processes = readPorcessFile(argv[1]);

    // TODO create scheduler with the proper algorithm

    sync_clk();
    sendProcessToScheduler();

    clear_resources(0);

    return EXIT_SUCCESS;
}


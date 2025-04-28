#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <bits/signum-arch.h>
#include "clk.h"
#include "pcb.h"
#include "queue.h"
#include "scheduler.h"
#include "headers.h"
#include "min_heap.h"
#include "process_generator.h"
#include "colors.h"
#include "shared_mem.h"
extern int total_busy_time;
extern int scheduler_type;
extern finishedProcessInfo** finished_process_info;
extern int finished_processes_count;

// compare function for priority queue
int compare_processes(const void* p1, const void* p2)
{
    PCB* process1 = (PCB*)p1;
    PCB* process2 = (PCB*)p2;
    if (scheduler_type == HPF)
    {
        if (process1->priority != process2->priority)
        {
            return process1->priority - process2->priority;
        }

        // If priorities are equal return that come first
        return process1->arrival_time - process2->arrival_time;
    }
    else // SRTN
    {
        if (process1->remaining_time != process2->remaining_time)
        {
            return process1->remaining_time - process2->remaining_time;
        }

        // If priorities are equal return that come first
        return process1->arrival_time - process2->arrival_time;
    }
}

// HPF algorithm
PCB* hpf(min_heap_t* ready_queue, int current_time)
{
    if (!min_heap_is_empty(ready_queue))
    {
        PCB* next_process = min_heap_extract_min(ready_queue);
        next_process->status = RUNNING;
        next_process->waiting_time = current_time - next_process->arrival_time;
        // assuming that any process is initially having start time -1
        if (next_process->start_time == -1)
        {
            next_process->start_time = current_time;
        }
        log_process_state(next_process, "started", current_time);
        kill(next_process->pid,SIGCONT);
        return next_process;
    }
    return NULL;
}

PCB* srtn(min_heap_t* ready_queue)
{
    int current_time = get_clk();
    if (!min_heap_is_empty(ready_queue))
    {
        PCB* next_process = min_heap_extract_min(ready_queue);
        next_process->status = RUNNING;
        if (next_process->last_run_time == -1)
        {
            next_process->waiting_time = current_time - next_process->arrival_time;
        }
        else next_process->waiting_time += current_time - next_process->last_run_time;

        // assuming that any process is initially having start time -1
        if (next_process->start_time == -1)
        {
            log_process_state(next_process, "started", current_time);
            next_process->start_time = current_time;
            next_process->response_time = next_process->start_time - next_process->arrival_time;
        }
        else
            log_process_state(next_process, "resumed", current_time);
        return next_process;
    }
    return NULL;
}


// RR algorithm
PCB* rr(Queue* ready_queue, int current_time)
{
    if (!isQueueEmpty(ready_queue))
    {
        PCB* next_process = dequeue(ready_queue);
        next_process->status = RUNNING;
        next_process->waiting_time = (current_time - next_process->arrival_time) - (next_process->runtime - next_process
            ->remaining_time);
        if (next_process->start_time == -1)
        {
            next_process->start_time = current_time;
            next_process->response_time = current_time - next_process->arrival_time;
            log_process_state(next_process, "started", current_time);
        }
        else
        {
            log_process_state(next_process, "resumed", current_time);
        }

        return next_process;
    }
    return NULL;
}

// Update log_process_state to handle more states
void log_process_state(PCB* process, char* state, int time)
{
    if (strcmp(state, "started") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
            time, process->pid, state, process->arrival_time, process->runtime,
            0, process->waiting_time,
            (time - process->arrival_time), /* Turnaround time */
            (process->runtime > 0) ? ((float)(time - process->arrival_time) / process->runtime) : 0.0); /* Weighted turnaround time */

        printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d started at time %d\n"ANSI_COLOR_RESET,
               process->pid, time);
    }
    else if (strcmp(state, "finished") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                time, process->pid, state, process->arrival_time, process->runtime,
                0, process->waiting_time,
                (time - process->arrival_time), // Turnaround time
                (float)(time - process->arrival_time) / process->runtime); // Weighted turnaround time

        printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d finished at time %d\n"ANSI_COLOR_RESET,
               process->pid, time);
    }
    else if (strcmp(state, "resumed") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, process->pid, state, process->arrival_time, process->runtime,
                process->remaining_time, process->waiting_time);

        printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d resumed at time %d\n"ANSI_COLOR_RESET,
               process->pid, time);
    }
    else if (strcmp(state, "preempted") == 0 || strcmp(state, "blocked") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, process->pid, state, process->arrival_time, process->runtime,
                process->remaining_time, process->waiting_time);

        printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d %s at time %d\n"ANSI_COLOR_RESET,
               process->pid, state, time);
    }
    else
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, process->pid, state, process->arrival_time, process->runtime,
                process->remaining_time, process->waiting_time);
    }

    fflush(log_file);
}

void generate_statistics()
{
    // Return early if no finished processes
    if (finished_processes_count == 0) return;

    // Allocate memory for tracking WTA values
    float** wta_values = (float**)malloc(finished_processes_count * sizeof(float*));
    if (!wta_values)
    {
        perror("Failed to allocate memory for wta_values");
        return; // Return if allocation fails
    }

    for (int i = 0; i < finished_processes_count; ++i)
    {
        wta_values[i] = NULL;
    }

    float total_wait = 0;
    float total_wta = 0;
    float total_ta = 0;

    int total_execution_time = get_clk(); // Total simulation time

    // Loop through all finished processes
    for (int i = 0; i < finished_processes_count; i++)
    {
        // Check if the pointer is valid
        if (finished_process_info[i] == NULL)
        {
            fprintf(stderr, "[SCHEDULER] Error: finished_process_info[%d] is NULL\n", i);
            continue; // Skip this iteration
        }

        total_wait += finished_process_info[i]->waiting_time;
        total_ta += finished_process_info[i]->ta;
        total_wta += finished_process_info[i]->wta;

        // Store WTA values (not waiting times) for standard deviation calculation
        wta_values[i] = (float*)malloc(sizeof(float));
        if (!wta_values[i])
        {
            fprintf(stderr, "[SCHEDULER] Failed to allocate memory for wta_values[%d]\n", i);
            continue;
        }
        *wta_values[i] = finished_process_info[i]->wta;
    }

    float avg_wait = total_wait / finished_processes_count;
    float avg_wta = total_wta / finished_processes_count;

    // Calculate standard deviation for WTA
    float sum_squared_diff = 0;
    for (int i = 0; i < finished_processes_count; i++)
    {
        if (wta_values[i] != NULL)
        {
            // Add null check
            float diff = (*wta_values[i]) - avg_wta;
            sum_squared_diff += diff * diff;
        }
    }
    float std_wta = sqrt(sum_squared_diff / finished_processes_count);

    // Calculate CPU utilization
    float cpu_utilization = ((float)(total_busy_time) / total_execution_time) * 100;

    // Write to performance file
    FILE* perf_file = fopen("scheduler.perf", "w");
    if (perf_file)
    {
        // Check if file opened successfully
        fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_utilization);
        fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
        fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
        fprintf(perf_file, "Std WTA = %.2f\n", std_wta);
        fclose(perf_file);
    }
    else
    {
        perror("Failed to open scheduler.perf");
    }

    // Free individual wta_values allocations first
    for (int i = 0; i < finished_processes_count; ++i)
    {
        if (wta_values[i] != NULL)
        {
            free(wta_values[i]);
        }
    }

    free(wta_values);
}
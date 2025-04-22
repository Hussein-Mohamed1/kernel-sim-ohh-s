#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pcb.h"
#include "scheduler.h"


// compare function for periorty queue
int compare_processes(const void* p1, const void* p2)
{
    PCB* process1 = (PCB*)p1;
    PCB* process2 = (PCB*)p2;


    if (process1->priority != process2->priority)
    {
        return process1->priority - process2->priority;
    }

    // If priorities are equal return that come first
    return process1->arrival_time - process2->arrival_time;
}

// Log process state changes
void log_process_state(PCB* process, char* state, int time)
{
    if (log_file == NULL)
    {
        log_file = fopen("scheduler.log", "w");
        fprintf(log_file, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");
    }

    if (strcmp(state, "started") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, process->id, state, process->arrival_time, process->runtime,
                process->remaining_time, process->waiting_time);
    }
    else if (strcmp(state, "finished") == 0)
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                time, process->id, state, process->arrival_time, process->runtime,
                0, process->waiting_time,
                (time - process->arrival_time), // Turnaround time
                (float)(time - process->arrival_time) / process->runtime); // Weighted turnaround time
    }
    else
    {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                time, process->id, state, process->arrival_time, process->runtime,
                process->remaining_time, process->waiting_time);
    }

    fflush(log_file);
}


// Generate performance statistics
void generate_statistics()
{
    if (process_count == 0) return;


    float total_wait = 0;
    float total_wta = 0;
    float total_ta = 0;
    float* wta_values = (float*)malloc(process_count * sizeof(float));

    int cpu_idle_time = 0;
    int total_execution_time = current_time; // Total simulation time

    for (int i = 0; i < process_count; i++)
    {
        PCB* process = finished_processes[i];
        int ta = process->finish_time - process->arrival_time;
        float wta = (float)ta / process->runtime;

        total_wait += process->waiting_time;
        total_ta += ta;
        total_wta += wta;
        wta_values[i] = wta;
    }

    float avg_wait = total_wait / process_count;
    float avg_wta = total_wta / process_count;

    // Calculate standard deviation for WTA
    float sum_squared_diff = 0;
    for (int i = 0; i < process_count; i++)
    {
        float diff = wta_values[i] - avg_wta;
        sum_squared_diff += diff * diff;
    }
    float std_wta = sqrt(sum_squared_diff / process_count);

    // Calculate CPU utilization (assuming idle time is properly tracked)
    float cpu_utilization = ((float)(total_execution_time - cpu_idle_time) / total_execution_time) * 100;

    // Write to performance file
    FILE* perf_file = fopen("scheduler.perf", "w");
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_utilization);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
    fprintf(perf_file, "Std WTA = %.2f\n", std_wta);
    fclose(perf_file);

    free(wta_values);
}

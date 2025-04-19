#include "stats.h"
#include <stdlib.h>
#include <math.h>

// Array to store completed process information
typedef struct {
    int waiting_time;
    double weighted_turnaround;
} ProcessStats;

static ProcessStats *completed_stats = NULL;
static int stats_count = 0;

// Add completed process statistics
static void add_process_stats(int waiting_time, double weighted_turnaround) {
    stats_count++;
    completed_stats = realloc(completed_stats, stats_count * sizeof(ProcessStats));
    
    completed_stats[stats_count-1].waiting_time = waiting_time;
    completed_stats[stats_count-1].weighted_turnaround = weighted_turnaround;
}

// Update statistics
void update_statistics(Node *ready_queue, PCB *running_process, 
                      int total_processes, int completed_processes, 
                      int current_time, int idle_time,
                      double *cpu_utilization, double *avg_waiting_time, 
                      double *avg_weighted_turnaround, double *std_dev_weighted_turnaround) {
    // Calculate CPU utilization
    int busy_time = current_time - idle_time;
    *cpu_utilization = (double)busy_time / current_time * 100;
    
    // If we have completed processes, calculate averages
    if (completed_processes > 0) {
        int total_waiting_time = 0;
        double total_weighted_turnaround = 0;
        
        for (int i = 0; i < stats_count; i++) {
            total_waiting_time += completed_stats[i].waiting_time;
            total_weighted_turnaround += completed_stats[i].weighted_turnaround;
        }
        
        *avg_waiting_time = (double)total_waiting_time / stats_count;
        *avg_weighted_turnaround = total_weighted_turnaround / stats_count;
        
        // Calculate standard deviation
        double sum_squared_diff = 0;
        for (int i = 0; i < stats_count; i++) {
            sum_squared_diff += pow(completed_stats[i].weighted_turnaround - *avg_weighted_turnaround, 2);
        }
        *std_dev_weighted_turnaround = sqrt(sum_squared_diff / stats_count);
    }
}

// Write performance metrics to file
void write_logs(double cpu_utilization, double avg_waiting_time, 
               double avg_weighted_turnaround, double std_dev_weighted_turnaround) {
    FILE *perf_file = fopen("scheduler.perf", "w");
    if (perf_file == NULL) {
        perror("Error opening performance file");
        exit(1);
    }
    
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_utilization);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_weighted_turnaround);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_waiting_time);
    fprintf(perf_file, "Std WTA = %.2f\n", std_dev_weighted_turnaround);
    
    fclose(perf_file);
    
    // Free allocated memory
    if (completed_stats != NULL) {
        free(completed_stats);
        completed_stats = NULL;
        stats_count = 0;
    }
}
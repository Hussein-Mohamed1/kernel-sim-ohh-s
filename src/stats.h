#ifndef STATS_H
#define STATS_H

#include "pcb.h"
#include <stdio.h>

// Function prototypes for statistics operations
void update_statistics(Node *ready_queue, PCB *running_process, 
                      int total_processes, int completed_processes, 
                      int current_time, int idle_time,
                      double *cpu_utilization, double *avg_waiting_time, 
                      double *avg_weighted_turnaround, double *std_dev_weighted_turnaround);
                      
void write_logs(double cpu_utilization, double avg_waiting_time, 
               double avg_weighted_turnaround, double std_dev_weighted_turnaround);

#endif // STATS_H
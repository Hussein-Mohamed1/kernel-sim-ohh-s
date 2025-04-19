#include "scheduler.h"

// Initialize the scheduler
void initialize_scheduler(int alg, int quantum_val, FILE **log_file, int *msgq_id) {
    // Open log files
    *log_file = fopen("scheduler.log", "w");
    if (*log_file == NULL) {
        perror("Error opening log file");
        exit(1);
    }
    
    // Print header for log file
    fprintf(*log_file, "#At time x process y state arr w total z remain y wait k\n");
    
    // Initialize message queue
    key_t key = ftok("keyfile", 65);
    *msgq_id = msgget(key, 0666 | IPC_CREAT);
    if (*msgq_id == -1) {
        perror("Error in creating message queue");
        exit(1);
    }
    
    printf("Scheduler initialized with algorithm %d and quantum %d\n", alg, quantum_val);
}

// Schedule processes based on selected algorithm
void schedule(int algorithm, int quantum, Node **ready_queue, PCB **running_process, 
             FILE *log_file, int current_time) {
    // If there's a running process, check if it should be preempted
    if (*running_process != NULL) {
        // For SRTN, check if a process with shorter remaining time has arrived
        if (algorithm == SRTN) {
            Node *temp = *ready_queue;
            while (temp != NULL) {
                if (temp->process.remaining_time < (*running_process)->remaining_time) {
                    stop_process(*running_process, ready_queue, running_process, log_file, current_time);
                    break;
                }
                temp = temp->next;
            }
        }
        // For RR, check if quantum is over
        else if (algorithm == RR) {
            int elapsed = current_time - (*running_process)->last_run_time;
            if (elapsed >= quantum && *ready_queue != NULL) {
                stop_process(*running_process, ready_queue, running_process, log_file, current_time);
            }
        }
        // HPF is non-preemptive, so no need to check for preemption
    }
    
    // If no process is running, select the next one
    if (*running_process == NULL && *ready_queue != NULL) {
        PCB *next_process = NULL;
        
        switch (algorithm) {
            case HPF:
                next_process = get_next_process_hpf(ready_queue);
                break;
            case SRTN:
                next_process = get_next_process_srtn(ready_queue);
                break;
            case RR:
                next_process = get_next_process_rr(ready_queue);
                break;
            default:
                printf("Unknown scheduling algorithm\n");
                exit(1);
        }
        
        if (next_process != NULL) {
            start_process(next_process, running_process, log_file, current_time);
        }
    }
}

// Cleanup resources
void cleanup(FILE *log_file, int msgq_id, Node **ready_queue, PCB **running_process) {
    // Close log file
    if (log_file != NULL) {
        fclose(log_file);
    }
    
    // Remove message queue
    msgctl(msgq_id, IPC_RMID, NULL);
    
    // Free allocated memory for any remaining processes
    while (*ready_queue != NULL) {
        Node *temp = *ready_queue;
        *ready_queue = (*ready_queue)->next;
        free(temp);
    }
    
    if (*running_process != NULL) {
        free(*running_process);
        *running_process = NULL;
    }
    
    printf("Scheduler cleaned up\n");
}

// Main scheduler function
void run_scheduler() {
    sync_clk();
    
    // Variables
    int algorithm;
    int quantum = 1;
    int current_time = 0;
    int total_processes = 0;
    int completed_processes = 0;
    int idle_time = 0;
    int msgq_id;
    double cpu_utilization = 0;
    double avg_waiting_time = 0;
    double avg_weighted_turnaround = 0;
    double std_dev_weighted_turnaround = 0;
    
    PCB *running_process = NULL;
    Node *ready_queue = NULL;
    FILE *log_file = NULL;
    
    // Parse command line arguments (algorithm and quantum)
    FILE *params_file = fopen("scheduler.params", "r");
    if (params_file == NULL) {
        perror("Error opening parameters file");
        exit(1);
    }
    
    fscanf(params_file, "%d", &algorithm);
    if (algorithm == RR) {
        fscanf(params_file, "%d", &quantum);
    }
    
    fclose(params_file);
    
    // Initialize scheduler
    initialize_scheduler(algorithm, quantum, &log_file, &msgq_id);
    
    ProcessMessage msg;
    
    // Main scheduling loop
    while (1) {
        // Get current time from the clock
        current_time = get_clk();
        
        // Check for new process arrivals
        if (msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), PROCESS_ARRIVED, IPC_NOWAIT) > 0) {
            PCB new_process;
            new_process.id = msg.id;
            new_process.arrival_time = msg.arrival_time;
            new_process.burst_time = msg.running_time;
            new_process.priority = msg.priority;
            
            add_process(&ready_queue, new_process, log_file, current_time);
            total_processes++;
        }
        
        // Check for process terminations
        check_process_termination(msgq_id, ready_queue, &running_process, 
                                 &completed_processes, log_file, current_time);
        
        // Schedule processes
        schedule(algorithm, quantum, &ready_queue, &running_process, log_file, current_time);
        
        // If all processes are completed, break the loop
        if (completed_processes >= total_processes && total_processes > 0) {
            break;
        }
        
        // If no process is running and ready queue is empty, increment idle time
        if (running_process == NULL && ready_queue == NULL) {
            idle_time++;
        }
        
        // Avoid busy waiting by sleeping briefly
        usleep(1000);
    }
    
    // Update and write statistics
    update_statistics(ready_queue, running_process, total_processes, completed_processes,
                     current_time, idle_time, &cpu_utilization, &avg_waiting_time,
                     &avg_weighted_turnaround, &std_dev_weighted_turnaround);
                     
    write_logs(cpu_utilization, avg_waiting_time, avg_weighted_turnaround, std_dev_weighted_turnaround);
    
    // Cleanup
    cleanup(log_file, msgq_id, &ready_queue, &running_process);
    
    // Release the clock resources
    destroy_clk(0);
}
#include "process.h"
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

// Log process state changes
void log_process_state(PCB *process, char *state, FILE *log_file, int current_time) {
    int wait_time = current_time - process->arrival_time - (process->burst_time - process->remaining_time);
    
    if (strcmp(state, "arrived") == 0) {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                current_time, process->id, state, process->arrival_time, 
                process->burst_time, process->remaining_time, 0);
    }
    else if (strcmp(state, "finished") == 0) {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                current_time, process->id, state, process->arrival_time, 
                process->burst_time, 0, wait_time,
                process->turnaround_time, process->weighted_turnaround);
    }
    else {
        fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n",
                current_time, process->id, state, process->arrival_time, 
                process->burst_time, process->remaining_time, wait_time);
    }
}

// Add a new process to the scheduler
void add_process(Node **ready_queue, PCB process, FILE *log_file, int current_time) {
    process.status = READY;
    process.pid = 0;
    process.remaining_time = process.burst_time;
    process.waiting_time = 0;
    process.start_time = 0;
    process.last_run_time = 0;
    process.completion_time = 0;
    
    // Log process arrival
    log_process_state(&process, "arrived", log_file, current_time);
    
    // Add to ready queue
    add_to_ready_queue(ready_queue, process);
}

// Start a process
void start_process(PCB *process, PCB **running_process, FILE *log_file, int current_time) {
    if (process == NULL) return;
    
    process->status = RUNNING;
    
    // If first time running, set the start time
    if (process->start_time == 0) {
        process->start_time = current_time;
        process->response_time = current_time - process->arrival_time;
        log_process_state(process, "started", log_file, current_time);
    } else {
        log_process_state(process, "resumed", log_file, current_time);
    }
    
    process->last_run_time = current_time;
    
    // Fork the process if not already started
    if (process->pid == 0) {
        int pid = fork();
        
        if (pid == 0) {
            // Child process
            // Execute the process code (simulation)
            char id_str[10], remaining_str[10];
            sprintf(id_str, "%d", process->id);
            sprintf(remaining_str, "%d", process->remaining_time);
            
            execl("./process.out", "process.out", id_str, remaining_str, NULL);
            perror("Failed to execute process");
            exit(1);
        } else if (pid > 0) {
            // Parent process
            process->pid = pid;
        } else {
            perror("Failed to fork process");
            exit(1);
        }
    } else {
        // Resume the process by sending SIGCONT
        kill(process->pid, SIGCONT);
    }
    
    *running_process = process;
}

// Stop a process
void stop_process(PCB *process, Node **ready_queue, PCB **running_process, FILE *log_file, int current_time) {
    if (process == NULL) return;
    
    // Only stop if it's running
    if (process->status == RUNNING) {
        // Update remaining time
        int elapsed = current_time - process->last_run_time;
        process->remaining_time -= elapsed;
        if (process->remaining_time < 0) process->remaining_time = 0;
        
        // Send SIGSTOP to pause the process
        kill(process->pid, SIGSTOP);
        
        // Update process state
        process->status = READY;
        
        // Log the process stopping
        log_process_state(process, "stopped", log_file, current_time);
        
        // Add back to ready queue
        add_to_ready_queue(ready_queue, *process);
        
        *running_process = NULL;
    }
}

// Handle process termination
void handle_process_termination(int pid, Node *ready_queue, PCB **running_process, 
                               int *completed_processes, FILE *log_file, int current_time) {
    PCB *process = get_process_by_pid(ready_queue, *running_process, pid);
    
    if (process != NULL) {
        // Update remaining time
        process->remaining_time = 0;
        
        // Update process statistics
        process->completion_time = current_time;
        process->turnaround_time = process->completion_time - process->arrival_time;
        process->weighted_turnaround = (float)process->turnaround_time / process->burst_time;
        process->status = TERMINATED;
        
        // Log process termination
        log_process_state(process, "finished", log_file, current_time);
        
        // Update global statistics
        (*completed_processes)++;
        
        // Clear the running process if it's this one
        if (*running_process != NULL && (*running_process)->pid == pid) {
            *running_process = NULL;
        }
        
        // Wait for the process to prevent zombies
        waitpid(pid, NULL, 0);
    }
}

// Check if any process has terminated
int check_process_termination(int msgq_id, Node *ready_queue, PCB **running_process, 
                             int *completed_processes, FILE *log_file, int current_time) {
    ProcessMessage msg;
    
    // Check for process termination messages
    if (msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), PROCESS_FINISHED, IPC_NOWAIT) > 0) {
        handle_process_termination(msg.pid, ready_queue, running_process, 
                                  completed_processes, log_file, current_time);
        return 1;
    }
    
    return 0;
}
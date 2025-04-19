#include "queue.h"
#include <stdlib.h>

// Add a process to the ready queue
void add_to_ready_queue(Node **ready_queue, PCB process) {
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) {
        perror("Failed to allocate memory for process node");
        exit(1);
    }
    
    new_node->process = process;
    new_node->next = NULL;
    
    // If the queue is empty
    if (*ready_queue == NULL) {
        *ready_queue = new_node;
        return;
    }
    
    // Add to the end of the queue
    Node *temp = *ready_queue;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = new_node;
}

// Remove a process from the ready queue
void remove_from_ready_queue(Node **ready_queue, PCB *process) {
    if (*ready_queue == NULL) return;
    
    Node *temp = *ready_queue;
    Node *prev = NULL;
    
    // If the first node is the one to be removed
    if (temp != NULL && temp->process.id == process->id) {
        *ready_queue = temp->next;
        *process = temp->process;
        free(temp);
        return;
    }
    
    // Search for the node to be removed
    while (temp != NULL && temp->process.id != process->id) {
        prev = temp;
        temp = temp->next;
    }
    
    // If not found
    if (temp == NULL) return;
    
    // Unlink the node
    prev->next = temp->next;
    *process = temp->process;
    free(temp);
}

// Get a process by its PID
PCB* get_process_by_pid(Node *ready_queue, PCB *running_process, int pid) {
    if (running_process != NULL && running_process->pid == pid) {
        return running_process;
    }
    
    Node *temp = ready_queue;
    while (temp != NULL) {
        if (temp->process.pid == pid) {
            return &(temp->process);
        }
        temp = temp->next;
    }
    
    return NULL;
}

// Get the next process based on HPF algorithm
PCB* get_next_process_hpf(Node **ready_queue) {
    if (*ready_queue == NULL) return NULL;
    
    Node *temp = *ready_queue;
    Node *highest_priority = temp;
    
    // Find the highest priority process
    while (temp != NULL) {
        if (temp->process.priority < highest_priority->process.priority) {
            highest_priority = temp;
        }
        temp = temp->next;
    }
    
    PCB *process = (PCB*)malloc(sizeof(PCB));
    *process = highest_priority->process;
    remove_from_ready_queue(ready_queue, process);
    
    return process;
}

// Get the next process based on SRTN algorithm
PCB* get_next_process_srtn(Node **ready_queue) {
    if (*ready_queue == NULL) return NULL;
    
    Node *temp = *ready_queue;
    Node *shortest = temp;
    
    // Find the process with shortest remaining time
    while (temp != NULL) {
        if (temp->process.remaining_time < shortest->process.remaining_time) {
            shortest = temp;
        }
        temp = temp->next;
    }
    
    PCB *process = (PCB*)malloc(sizeof(PCB));
    *process = shortest->process;
    remove_from_ready_queue(ready_queue, process);
    
    return process;
}

// Get the next process based on RR algorithm
PCB* get_next_process_rr(Node **ready_queue) {
    if (*ready_queue == NULL) return NULL;
    
    PCB *process = (PCB*)malloc(sizeof(PCB));
    *process = (*ready_queue)->process;
    remove_from_ready_queue(ready_queue, process);
    
    return process;
}

// Get the size of the queue
int queue_size(Node *ready_queue) {
    int count = 0;
    Node *temp = ready_queue;
    
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    
    return count;
}
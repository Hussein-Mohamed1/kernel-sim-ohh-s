#pragma once

// Process Control Block (PCB)
typedef struct {
    int id;               
    int arrival_time;      
    int runtime;         
    int remaining_time;
    int priority;           
    int waiting_time;       
    int start_time;      
    int last_run_time;     
    int finish_time;  
    int response_time;     
    int turnaround_time;  
    float weighted_turnaround; 
    int status;             
    int pid;             
} PCB;

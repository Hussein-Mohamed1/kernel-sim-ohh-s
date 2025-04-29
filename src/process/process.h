#pragma once

#include <signal.h>
#include "shared_mem.h"

// Add shared memory key definition
#define SHM_KEY 400
#define MAX_PROCESSES 100

void sigIntHandler(int signum);
void sigStpHandler(int signum);
void sigContHandler(int signum);
void run_process(int runtime);
int get_time_to_run(int shmid, pid_t pid);
void update_process_status(int proc_shmid, pid_t pid, int status);
int get_process_status(int proc_shmid);
int get_time_to_run(int proc_shmid, pid_t pid);
process_info_t get_process_info(int proc_shmid);
void sigIntHandler(int signum);
void sigStpHandler(int signum);
void sigContHandler(int signum);
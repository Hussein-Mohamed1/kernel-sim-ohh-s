#include "scheduler.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/shm.h>

#include "clk.h"
#include "scheduler_utils.h"
#include "min_heap.h"
#include "queue.h"
#include <sys/types.h>
#include <sys/wait.h>
#include "../shared_mem/shared_mem.h"

#include "headers.h"
#include "colors.h"

#include "buddy.h"
extern int total_busy_time;
extern finishedProcessInfo** finished_process_info;
// Use pointers for both possible queue types
min_heap_t* min_heap_queue = NULL;
Queue* rr_queue = NULL;

extern int msgid;
extern int scheduler_type;
extern int quantum;
extern int finished_processes_count;
int process_shm_id = -1; // Shared memory ID

void wait_for_process_state_change(pid_t pid, int expected_state);

void run_scheduler()
{
    signal(SIGINT, scheduler_cleanup);
    signal(SIGSTOP, scheduler_cleanup);
    signal(SIGTERM, scheduler_cleanup);
    signal(SIGCHLD, child_cleanup);
    sync_clk();

    if (init_scheduler() == -1)
    {
        fprintf(stderr, ANSI_COLOR_GREEN"[SCHEDULER] Failed to initialize scheduler\n"ANSI_COLOR_RESET);
        return;
    }
    int start_process_time = 0;
    int end_process_time = 0;

    while (1)
    {
        // Check for new processes
        int recv_status = receive_processes();

        // If no more processes and message queue closed, exit
        if ((recv_status == -2 || recv_status == -1) && !process_count)
        {
            break;
        }

        // Select process based on scheduling algorithm
        PCB* next_process = NULL;

        if (scheduler_type == HPF)
        {
            next_process = hpf(min_heap_queue, get_clk());
        }
        else if (scheduler_type == SRTN)
        {
            next_process = srtn(min_heap_queue);
        }
        else if (scheduler_type == RR)
        {
            next_process = rr(rr_queue, get_clk());
        }

        if (next_process == NULL)
        {
            continue;
        }

        // Prepare time slice
        int time_slice;
        if (scheduler_type == RR)
        {
            time_slice = (next_process->remaining_time < quantum) ? next_process->remaining_time : quantum;
        }
        else
        {
            time_slice = next_process->remaining_time;
        }

        // Set up process for execution
        running_process = next_process;
        int start_time = get_clk();

        // Write control info and resume process
        write_process_control(process_shm_id, running_process->pid,
                              time_slice, PROC_RUNNING);
                              
        // Add diagnostic output to track the process signal sequence
        if (DEBUG)
            printf(ANSI_COLOR_YELLOW"[SCHEDULER] Sending SIGCONT to process %d with time_slice=%d\n"ANSI_COLOR_RESET, 
                   running_process->pid, time_slice);
                   
        kill(running_process->pid, SIGCONT);
        
        // Give the process a moment to react to the signal
        usleep(5000);
        
        // Verify process received control info
        process_control_t ctrl_verify = read_process_control(process_shm_id, running_process->pid);
        if (DEBUG)
            printf(ANSI_COLOR_YELLOW"[SCHEDULER] Process %d status after SIGCONT: state=%d\n"ANSI_COLOR_RESET,
                   running_process->pid, ctrl_verify.state);

        // Wait for process to complete time slice
        while (1)
        {
            if (running_process == NULL) break;

            process_control_t ctrl = read_process_control(process_shm_id, running_process->pid);

            // Check if process finished its time slice
            if (ctrl.state == PROC_IDLE || ctrl.state == PROC_FINISHED)
            {
                break;
            }

            // For SRTN, check for preemption);
            if (scheduler_type == SRTN)
            {
                receive_processes(); // Check for new processes
                if (running_process == NULL) break;
                PCB* shortest = min_heap_get_min(min_heap_queue);
                if (shortest && shortest->remaining_time)
                {
                    // Calculate the actual remaining time of the running process
                    int current_time = get_clk();
                    int executed_time = current_time - start_time;
                    int actual_remaining = running_process->remaining_time - executed_time;

                    if (shortest->remaining_time < actual_remaining)
                    {
                        printf(ANSI_COLOR_BOLD_RED"PREMPTING"ANSI_COLOR_RESET);
                        // Wait for process to acknowledge the signal
                        write_process_control(process_shm_id, running_process->pid, 0, PROC_IDLE);

                        // Preempt current process
                        kill(running_process->pid, SIGTSTP);

                        if (DEBUG)
                            printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d successfully stopped\n"ANSI_COLOR_RESET,
                                   running_process->pid);

                        // Update process state and requeue
                        running_process->remaining_time = actual_remaining;
                        running_process->status = PROC_IDLE;
                        running_process->last_run_time = get_clk();

                        log_process_state(running_process, "stopped", get_clk());
                        min_heap_insert(min_heap_queue, running_process);
                        running_process = NULL;
                        break;
                    }
                }
            }
            // else receive_processes();
        }

        // Process finished its time slice
        if (running_process && running_process->remaining_time > 0)
        {
            int executed_time = get_clk() - start_time;
            running_process->remaining_time -= executed_time;

            // Check if process completed
            if (running_process->remaining_time <= 0 ||
                read_process_control(process_shm_id, running_process->pid).state == PROC_FINISHED)
            {
                // Process completed, wait for SIGCHLD handler to clean up
                // The handler will update statistics
                continue;
            }

            // Process still has work to do
            if (scheduler_type == RR)
            {
                // Put process back in RR queue
                running_process->status = PROC_IDLE;
                running_process->last_run_time = get_clk();
                log_process_state(running_process, "stopped", get_clk());
                enqueue(rr_queue, running_process);
                running_process = NULL;
            }
            else if (scheduler_type == SRTN)
            {
                // Reinsert into SRTN queue
                running_process->status = PROC_IDLE;
                running_process->last_run_time = get_clk();
                log_process_state(running_process, "stopped", get_clk());
                min_heap_insert(min_heap_queue, running_process);
                running_process = NULL;
            }
        }
    }

    // Must Be called before the clock is destroyed !!!
    generate_statistics();
    scheduler_cleanup(0);
    exit(0);
}

int receive_processes(void)
{
    if (msgid == -1)
        return -1;

    PCB received_pcb;
    size_t recv_val = msgrcv(msgid, &received_pcb, sizeof(PCB), 1, IPC_NOWAIT);

    if (recv_val == -1)
    {
        if (errno == ENOMSG)
            return errno; // No message available
        else if (errno == EIDRM || errno == EINVAL)
        {
            // EIDRM: Queue was removed
            // EINVAL: Invalid queue ID (queue no longer exists)
            // printf("[SCHEDULER] Message queue has been closed or removed\n");
            return -2; // Special return value to indicate queue closure
        }
        else
        {
            perror("Error receiving message");
            return errno;
        }
    }

    while (recv_val != -1)
    {
        printf(
            ANSI_COLOR_GREEN"[SCHEDULER] Received process ID: %d, arrival time: %d, remaining_time: %d at %d\n"
            ANSI_COLOR_RESET,
            received_pcb.pid, received_pcb.arrival_time, received_pcb.remaining_time, get_clk());

        PCB* new_pcb = (PCB*)malloc(sizeof(PCB));
        if (!new_pcb)
        {
            perror("Failed to allocate memory for PCB");
            break;
        }
        *new_pcb = received_pcb; // shallow copy, doesnt matter

        // Allocate memory for the new process (Phase 2)
        new_pcb->memory_start = allocate_memory(new_pcb->pid, new_pcb->memsize);
        if (new_pcb->memory_start == -1)
        {
            fprintf(stderr, "PID %d: Memory allocation failed (size=%d)\n",
                    new_pcb->pid, new_pcb->memsize);
            free(new_pcb);
            return -1;
        }

        // Log allocation (Phase 2)
        log_memory_op(
            get_clk(), new_pcb->pid, new_pcb->memsize,
            new_pcb->memory_start,
            new_pcb->memory_start + new_pcb->memsize - 1,
            1 // 1 = allocation
        );

        if (scheduler_type == HPF || scheduler_type == SRTN)
            min_heap_insert(min_heap_queue, new_pcb);
        else if (scheduler_type == RR)
            enqueue(rr_queue, new_pcb);

        process_count++;
        recv_val = msgrcv(msgid, &received_pcb, sizeof(PCB), 1, IPC_NOWAIT);

        if (recv_val == -1 && (errno == EIDRM || errno == EINVAL))
        {
            if (DEBUG)
                printf("[SCHEDULER] Message queue has been closed or removed during processing\n");
            return -2; // Queue was removed during processing
        }
    }

    return 0;
}

void scheduler_cleanup(int signum)
{
    if (DEBUG)
        printf(ANSI_COLOR_GREEN"[SCHEDULER] scheduler_cleanup CALLED\n"ANSI_COLOR_RESET);

    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    // Clean up shared memory
    cleanup_shared_memory(process_shm_id);
    process_shm_id = -1;

    // Free space used by buddy system
    destruct_buddy();

    // Cleanup memory resources if they still exist
    if (min_heap_queue)
    {
        // Free any remaining PCBs in the heap
        while (min_heap_queue->size > 0)
        {
            PCB* pcb = min_heap_extract_min(min_heap_queue);
            if (pcb != NULL)
                free(pcb);
        }
        destroy_min_heap(min_heap_queue);
        min_heap_queue = NULL;
    }

    if (rr_queue)
    {
        // Free any remaining PCBs in the queue
        while (!isQueueEmpty(rr_queue))
        {
            PCB* pcb = dequeue(rr_queue);
            if (pcb != NULL)
                free(pcb);
        }
        free(rr_queue);
        rr_queue = NULL;
    }

    // Don't try to remove the message queue that's already been removed
    if (msgid != -1)
    {
        // Check if queue still exists
        struct msqid_ds queue_info;
        if (msgctl(msgid, IPC_STAT, &queue_info) != -1)
        {
            msgctl(msgid, IPC_RMID, NULL);
        }
        msgid = -1;
    }

    for (int i = 0; i < MAX_PROCESSES; i++)
        if (finished_process_info[i] != NULL)
        {
            free(finished_process_info[i]);
            finished_process_info[i] = NULL;
        }

    if (finished_process_info != NULL)
    {
        free(finished_process_info);
        finished_process_info = NULL;
    }

    if (DEBUG)
        printf(ANSI_COLOR_GREEN"[SCHEDULER] scheduler_cleanup FINISHED \n"ANSI_COLOR_RESET);

    // if (signum != 0)
    // {
    destroy_clk(0);
    exit(0);
    // }
}

void child_cleanup()
{
    if (running_process == NULL) return;;

    // sync_clk();
    signal(SIGCHLD, child_cleanup);
    if (DEBUG)
        printf(ANSI_COLOR_GREEN"[SCHEDULER] CHILD_CLEANUP CALLED\n"ANSI_COLOR_RESET);

    if (running_process)
    {
        int current_time = get_clk();
        running_process->finish_time = current_time;
        running_process->remaining_time = 0;
        log_process_state(running_process, "finished", current_time);

        // Free the memory allocated for the process (phase 2)
        log_memory_op(
            get_clk(), running_process->pid, running_process->memsize,
            running_process->memory_start,
            running_process->memory_start + running_process->memsize - 1,
            0 // 0 = free operation
        );
        free_memory(running_process->pid);


        if (finished_processes_count < MAX_INPUT_PROCESSES)
        {
            if (finished_process_info[finished_processes_count] == NULL)
            {
                finished_process_info[finished_processes_count] = (finishedProcessInfo*)malloc(
                    sizeof(finishedProcessInfo));
                if (!finished_process_info[finished_processes_count])
                {
                    perror("Failed to malloc finished_process_info");
                }
                else
                {
                    // Only access if malloc succeeded
                    finished_process_info[finished_processes_count]->ta = current_time - running_process
                        ->arrival_time;
                    finished_process_info[finished_processes_count]->wta =
                        (running_process->runtime > 0)
                            ? ((float)(finished_process_info[finished_processes_count]->ta) / running_process->runtime)
                            : 0.0;
                    finished_process_info[finished_processes_count]->waiting_time = running_process->waiting_time;
                }
            }

            process_count--;
            finished_processes_count++;
        }
        else
        {
            printf(ANSI_COLOR_GREEN"[SCHEDULER] WARNING: Exceeded maximum number of processes!\n"ANSI_COLOR_RESET);
        }

        free(running_process);
        running_process = NULL;
    }
    else
    {
        printf("[SCHEDULER] Requested to cleanup none????\n");
    }
    if (DEBUG)
        printf(ANSI_COLOR_GREEN"[SCHEDULER] CHILD_CLEANUP FINISHED\n"ANSI_COLOR_RESET);
}

int init_scheduler()
{
    int current_time = get_clk();
    process_count = 0;
    running_process = NULL;

    // Initialize shared memory
    process_shm_id = create_shared_memory(SHM_KEY);
    if (process_shm_id == -1)
    {
        perror("Failed to create shared memory");
        return -1;
    }

    if (scheduler_type == HPF || scheduler_type == SRTN)
    {
        min_heap_queue = create_min_heap(MAX_INPUT_PROCESSES, compare_processes);
        if (min_heap_queue == NULL)
        {
            perror("Failed to create min_heap_queue");
            return -1;
        }
    }
    else if (scheduler_type == RR)
    {
        rr_queue = (Queue*)malloc(sizeof(Queue));
        if (rr_queue == NULL)
        {
            perror("Failed to allocate memory for rr_queue");
            return -1;
        }
        initQueue(rr_queue, sizeof(PCB));
    }

    // Init IPC
    key_t key = ftok("process_generator", 65);
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1)
    {
        perror("Error getting message queue");
        return -1;
    }

    log_file = fopen("scheduler.log", "w");
    if (log_file == NULL)
    {
        perror("Failed to open log file");
        return -1;
    }
    fprintf(log_file, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");

    finished_processes_count = 0;
    finished_process_info = (finishedProcessInfo**)malloc(MAX_INPUT_PROCESSES * sizeof(finishedProcessInfo*));

    if (!finished_process_info)
    {
        perror("Failed to allocate memory for PCB");
        return -1;
    }

    // Initialize all pointers to NULL
    for (int i = 0; i < MAX_INPUT_PROCESSES; i++)
        finished_process_info[i] = NULL;


    init_buddy();

    if (DEBUG)
        printf(ANSI_COLOR_GREEN"[SCHEDULER] Scheduler initialized successfully at time %d\n"ANSI_COLOR_RESET,
               current_time);
    return 0;
}

void wait_for_process_state_change(pid_t pid, int expected_state)
{
    process_control_t ctrl;
    int attempts = 0;
    int max_attempts = 100; // Add a reasonable limit
    
    do
    {
        ctrl = read_process_control(process_shm_id, pid);

        if (ctrl.state == expected_state)
        {
            if (DEBUG)
                printf(ANSI_COLOR_GREEN"[SCHEDULER] Process %d changed to expected state %d\n"ANSI_COLOR_RESET,
                      pid, expected_state);
            return;
        }

        // Small wait between checks
        attempts++;
        if (attempts % 10 == 0)
        {
            fprintf(stderr, ANSI_COLOR_RED"[SCHEDULER] Process %d still in state %d (expected %d) after %d attempts\n"
                    ANSI_COLOR_RESET, pid, ctrl.state, expected_state, attempts);
            
            // Resend the signal after a few attempts
            if (expected_state == PROC_IDLE)
                kill(pid, SIGTSTP);
            else if (expected_state == PROC_RUNNING)
                kill(pid, SIGCONT);
        }
        
        if (attempts >= max_attempts) {
            fprintf(stderr, ANSI_COLOR_RED"[SCHEDULER] Giving up on waiting for process %d to change state\n"
                    ANSI_COLOR_RESET, pid);
            break;
        }
        
        usleep(1000); // Short sleep to prevent CPU hogging
    }
    while (1);

    return;
}

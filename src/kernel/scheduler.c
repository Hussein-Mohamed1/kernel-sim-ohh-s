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
#include "shared_mem.h"

#include "headers.h"
#include "colors.h"
extern int total_busy_time;
extern finishedProcessInfo** finished_process_info;
// Use pointers for both possible queue types
min_heap_t* min_heap_queue = NULL;
Queue* rr_queue = NULL;

extern int msgid;
extern int scheduler_type;
extern int quantum;
extern finishedProcessInfo** finished_process_info;
extern int finished_processes_count;
int process_shm_id = -1; // Shared memory ID

void run_scheduler()
{
    signal(SIGINT, scheduler_cleanup);
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


        int receive_status = receive_processes();
        if (receive_status == -2 && !process_count)
        {
            printf(
                ANSI_COLOR_GREEN"[SCHEDULER] Message queue has been closed. Terminating scheduler.\n"ANSI_COLOR_RESET);
            break; // Exit the scheduling loop
        }
        receive_processes();



        if (scheduler_type == HPF) // HPF
        {
            int crt_clk = get_clk();
            running_process = hpf(min_heap_queue, crt_clk);
            if (running_process == NULL) continue; // there is no process to run
            start_process_time = get_clk();
            int time_slice = running_process->remaining_time;

            // Write current clock as handshake
            write_process_info(process_shm_id, running_process->pid, time_slice, 1, crt_clk);

            running_process->remaining_time = 0;
            pid_t p_pid = running_process->pid;
            process_info_t process_info;

            printf(ANSI_COLOR_GREEN"[SCHEDULER] RUNNING PID %d for %d units\n"ANSI_COLOR_RESET,
                   running_process->pid, time_slice);
            kill(running_process->pid, SIGCONT);

            while ((process_info = read_process_info(process_shm_id, p_pid)).status)
            {
                usleep(1);
                receive_processes();
            }

            // Wait until the process is cleanedup
            while (running_process != NULL)
            {
                usleep(1000);
                receive_processes();
            }
            end_process_time = get_clk(); 
            total_busy_time += (end_process_time - start_process_time);
        }

        else if (scheduler_type == SRTN)
        {
            running_process = srtn(min_heap_queue);
            if (running_process == NULL) continue; // there is no process to run
            start_process_time = get_clk(); 
            pid_t p_pid = running_process->pid;
            int remaining_time = running_process->remaining_time;
            process_info_t process_info;
            int ran = 0;
            int preempt = 0;

            int crt_clk = get_clk();
            // Write current clock as handshake
            write_process_info(process_shm_id, running_process->pid, 1, 1, crt_clk);

            printf(ANSI_COLOR_GREEN"[SCHEDULER] RUNNING PID %d for SRTN scheduling\n"ANSI_COLOR_RESET,
                   running_process->pid);
            kill(running_process->pid, SIGCONT);

            // While the process has more time to run
            while (ran < remaining_time)
            {
                // Wait until the process finishes the time slice and during that check if any new process arrives
                while (read_process_info(process_shm_id, p_pid).status != 0)
                {
                    // Check if there are any newly arrived processes
                    // If we received new processes and the min heap is not empty, check for preemption
                    if (receive_processes() == 0 && !min_heap_is_empty(min_heap_queue))
                    {
                        PCB* shortest = (PCB*)min_heap_get_min(min_heap_queue);
                        if (shortest && shortest->remaining_time < remaining_time - ran)
                            // Preempt the current process
                            preempt = 1;
                    }
                    usleep(1);
                }

                ran++;

                if (running_process)
                {
                    // Process has more time to run
                    if (remaining_time > ran)
                    {
                        // If the process needs to be prempted then break
                        if (preempt) break;

                        // else
                        // Instruct process to run for another time unit
                        write_process_info(process_shm_id, running_process->pid, 1, 1, get_clk());
                        printf(
                            ANSI_COLOR_GREEN"[SCHEDULER] PID %d continued for another unit. %d/%d completed\n"
                            ANSI_COLOR_RESET,
                            running_process->pid, ran, running_process->remaining_time);
                    }
                    else
                    {
                        // Process completed its current time slice
                        printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d completed time slice\n"ANSI_COLOR_RESET,
                               running_process->pid);
                        break;
                    }
                }

                usleep(1000); // Small sleep to prevent CPU hogging
            }

            // handle preempting and process still exists and there is still time left
            if (preempt && running_process && (remaining_time - ran) > 0)
            {
                // Update remaining time and reinsert into min heap
                running_process->remaining_time -= ran;
                running_process->last_run_time = get_clk();
                running_process->status = READY;

                log_process_state(running_process, "stopped", get_clk()); // Add explicit preemption log

                // Update process status to paused
                write_process_info(process_shm_id, running_process->pid, 0, 0, crt_clk);
                kill(running_process->pid, SIGTSTP); // Stop the process

                // Wait gracefully until the process reports that it stopped
                while (read_process_info(process_shm_id, running_process->pid).status)
                {
                    usleep(1000);
                    receive_processes();
                }

                printf(
                    ANSI_COLOR_GREEN
                    "[SCHEDULER] PID %d preempted and reinserted into queue with %d units remaining\n"
                    ANSI_COLOR_RESET,
                    p_pid, running_process->remaining_time);

                // Reinsert the process into the min heap
                min_heap_insert(min_heap_queue, running_process);
                running_process = NULL;
            }
            // The process finished running
            else if ((remaining_time - ran) <= 0)
            {
                // Wait for the process to be cleaned up
                while (running_process != NULL)
                {
                    usleep(1000);
                    receive_processes();
                }
                printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d has completed execution\n"ANSI_COLOR_RESET, p_pid);
            }
            end_process_time = get_clk(); 
            total_busy_time += (end_process_time - start_process_time);
        }
        else if (scheduler_type == RR)
        {
            int crt_clk = get_clk();
            running_process = rr(rr_queue, crt_clk);
            if (running_process == NULL) continue; // there is no process to run
            start_process_time = get_clk();
            int remaining_time = running_process->remaining_time;
            int time_slice = (remaining_time < quantum) ? remaining_time : quantum;
            pid_t p_pid = running_process->pid;

            // Write current clock as handshake
            write_process_info(process_shm_id, running_process->pid, time_slice, 1, crt_clk);

            printf(ANSI_COLOR_GREEN"[SCHEDULER] Running PID %d for %d units (RR)\n"ANSI_COLOR_RESET,
                   running_process->pid, time_slice);

            // Continue the process
            kill(running_process->pid, SIGCONT);

            // Wait for the process to finish its time slice
            while (read_process_info(process_shm_id, p_pid).status)
            {
                usleep(1000);
                receive_processes(); // Check for new arrivals while waiting
            }

            if (running_process != NULL)
            {
                // Update process accounting
                remaining_time -= time_slice;
                running_process->last_run_time = get_clk();

                printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d finished time slice. Remaining time: %d\n"ANSI_COLOR_RESET,
                       running_process->pid, remaining_time);

                if (remaining_time <= 0)
                {
                    // Wait for the process to be cleaned up
                    while (running_process != NULL)
                    {
                        usleep(1000);
                        receive_processes();
                    }

                    printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d has completed execution\n"ANSI_COLOR_RESET, p_pid);
                }
                else
                {
                    // Process still has time remaining, put it back in the queue
                    running_process->status = READY;
                    running_process->remaining_time = remaining_time;
                    
                    log_process_state(running_process, "blocked", get_clk()); // Add log when process is blocked
                    
                    enqueue(rr_queue, running_process);
                    running_process = NULL;

                    printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d re-enqueued with %d units remaining\n"ANSI_COLOR_RESET,
                           p_pid, remaining_time);
                }
            }
            else { printf(ANSI_COLOR_GREEN"[SCHEDULER] PID %d has completed execution\n"ANSI_COLOR_RESET, p_pid); }
            end_process_time = get_clk(); 
            total_busy_time += (end_process_time - start_process_time);
        }
    }
    // Must Be called before the clock is destroyed !!!
    generate_statistics();
    destroy_clk(1);
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

        if (scheduler_type == HPF || scheduler_type == SRTN)
            min_heap_insert(min_heap_queue, new_pcb);
        else if (scheduler_type == RR)
            enqueue(rr_queue, new_pcb);

        process_count++;
        recv_val = msgrcv(msgid, &received_pcb, sizeof(PCB), 1, IPC_NOWAIT);

        if (recv_val == -1 && (errno == EIDRM || errno == EINVAL))
        {
            printf("[SCHEDULER] Message queue has been closed or removed during processing\n");
            return -2; // Queue was removed during processing
        }
    }

    return 0;
}

void scheduler_cleanup(int signum)
{
    printf(ANSI_COLOR_GREEN"[SCHEDULER] scheduler_cleanup CALLED\n"ANSI_COLOR_RESET);

    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    // Clean up shared memory
    cleanup_shared_memory(process_shm_id);
    process_shm_id = -1;

    // Cleanup memory resources if they still exist
    if (min_heap_queue)
    {
        // Free any remaining PCBs in the heap
        while (min_heap_queue->size > 0)
        {
            PCB* pcb = min_heap_extract_min(min_heap_queue);
            if (pcb)
                free(pcb);
        }
        free(min_heap_queue);
        min_heap_queue = NULL;
    }

    if (rr_queue)
    {
        // Free any remaining PCBs in the queue
        while (!isQueueEmpty(rr_queue))
        {
            PCB* pcb = dequeue(rr_queue);
            if (pcb)
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

    for (int i = 0; i < MAX_INPUT_PROCESSES; i++)
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

    printf(ANSI_COLOR_GREEN"[SCHEDULER] scheduler_cleanup FINISHED \n"ANSI_COLOR_RESET);

    // if (signum != 0)
    // {
    exit(0);
    // }
}

void child_cleanup()
{
    if (running_process == NULL) return;;

    sync_clk();
    signal(SIGCHLD, child_cleanup);
    printf(ANSI_COLOR_GREEN"[SCHEDULER] CHILD_CLEANUP CALLED\n"ANSI_COLOR_RESET);

    if (running_process)
    {
        int current_time = get_clk();
        running_process->finish_time = current_time;
        running_process->remaining_time = 0;
        log_process_state(running_process, "finished", current_time);
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
                        (running_process->runtime > 0) ? 
                        ((float)(finished_process_info[finished_processes_count]->ta) / running_process->runtime) : 
                        0.0;
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

    printf(ANSI_COLOR_GREEN"[SCHEDULER] Scheduler initialized successfully at time %d\n"ANSI_COLOR_RESET,
           current_time);
    return 0;
}
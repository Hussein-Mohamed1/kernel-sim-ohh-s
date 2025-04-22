#include "scheduler.h"
#include "clk.h"
#include "headers.h"
#include "process.h"

void run_scheduler()
{
    sync_clk();
    // TODO function to init a scheduler
    while (1)
    {
        current_time = get_clk();
        // receive_processes();
        PCB* next_process = hpf(ready_queue, running_process, current_time, finished_processes,
                                completed_process_count);
        if (next_process != NULL)
            run_process(next_process);

        // There are no ready processes to process
        if (completed_process_count == process_count && process_count > 0 &&
            min_heap_is_empty(ready_queue) && !running_process)
            break;
    }

    destroy_clk(0);
}

PCB* hpf(min_heap_t* ready_queue, PCB* running_process, int current_time, PCB** finished_processes,
         int completed_process_count)
{
    if (running_process && running_process->remaining_time <= 0)
    {
        running_process->status = TERMINATED;
        running_process->finish_time = current_time;
        running_process->waiting_time = (running_process->finish_time - running_process->arrival_time) - running_process
            ->runtime;
        finished_processes[completed_process_count++] = running_process;
        log_process_state(running_process, "finished", current_time);
        running_process = NULL;
    }
    if (!running_process && !min_heap_is_empty(ready_queue))
    {
        PCB* next_process = min_heap_extract_min(ready_queue);
        next_process->status = RUNNING;
        next_process->waiting_time = current_time - next_process->arrival_time;
        // assuming that any process is initially having start time -1
        if (next_process->start_time == -1)
        {
            next_process->start_time = current_time;
        }
        log_process_state(next_process, "started", current_time);
        // TODO call function to run the process in process.c
        return next_process;
    }
    return NULL;
}

PCB* rr(min_heap_t* ready_queue, PCB* running_process, int current_time, PCB** finished_processes,
        int completed_process_count)
{
    if (running_process && running_process->remaining_time <= 0)
    {
        running_process->status = TERMINATED;
        running_process->finish_time = current_time;
        running_process->waiting_time = (running_process->finish_time - running_process->arrival_time) - running_process
            ->runtime;
        finished_processes[completed_process_count++] = running_process;
        log_process_state(running_process, "finished", current_time);
        running_process = NULL;
    }
    if (!running_process && !min_heap_is_empty(ready_queue))
    {
        PCB* next_process = min_heap_extract_min(ready_queue);
        next_process->status = RUNNING;
        next_process->waiting_time = current_time - next_process->arrival_time;
        // assuming that any process is initially having start time -1
        if (next_process->start_time == -1)
        {
            next_process->start_time = current_time;
        }
        log_process_state(next_process, "started", current_time);
        // TODO call function to run the process in process.c
        return next_process;
    }
    return NULL;
}

#include "headers.h"


void run_scheduler()
{
    sync_clk();
    if(init_scheduler() == -1) {
        fprintf(stderr, "Failed to initialize scheduler\n");
        return;
    }

    while(1) {
        current_time = get_clk();
        // TODO call function that recieves processes from process generator
        // receive_processes();
        PCB * next_process = hpf(ready_queue , running_process , current_time  , completed_process_count);
        if(next_process != NULL) {
            // TODO function to run the process
            // run_process(next_process);
        }
        if (completed_process_count == process_count && process_count > 0 && 
            min_heap_is_empty(ready_queue) && !running_process) {
            break;
        }
    }
    generate_statistics();
    if(log_file) {
        fclose(log_file);
    }
    destroy_clk(0);
}

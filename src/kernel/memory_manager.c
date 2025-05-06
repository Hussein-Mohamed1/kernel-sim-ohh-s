#include "memory_manager.h"
#include <stdlib.h>
#include <string.h>
#include "colors.h"
#include "buddy.h"

typedef struct {
    struct buddy* memory;
    min_heap_t* waiting_list;
    int* id_to_offset_map;    // Map process ID to memory offset
    size_t* id_to_size_map;   // Map process ID to memory size
    int max_id;               // Max process ID
    
    // Fields for PID-to-ID mapping
    int* pid_to_id_map;       // Maps PIDs to process IDs
    int* id_to_pid_map;       // Maps process IDs to PIDs
    int max_pid;              // Maximum PID value
} memory_manager_t;

static memory_manager_t* mm = NULL;

typedef struct {
    processParameters params;        
    size_t size;   
} waiting_process_t;

// Comparator For Min Heap
static int waiting_process_compare(const void* a, const void* b) {
    const waiting_process_t* wa = (const waiting_process_t*)a;
    const waiting_process_t* wb = (const waiting_process_t*)b;
    
    // Compare based on memory size (smaller sizes get higher priority)
    if (wa->size != wb->size) {
        return wa->size < wb->size ? -1 : 1;
    }
    
    // If sizes are equal, compare based on arrival time
    return wa->params.arrival_time - wb->params.arrival_time;
}

// Global variable for memory log file
static FILE* memory_log_file = NULL;

// Helper function declarations
static inline bool is_power_of_2(unsigned int x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

static inline unsigned int next_power_of_2(unsigned int x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

void mm_initialize_memory_log()
{
    memory_log_file = fopen("memory.log", "w");
    if (memory_log_file == NULL)
    {
        perror("Failed to open memory log file");
        return;
    }
    
    fprintf(memory_log_file, "#At time x allocated y bytes for process z from i to j\n");
    fflush(memory_log_file);
}

void mm_log_memory_allocation(int time, int process_id, int size, int start_address, int end_address)
{
    if (memory_log_file == NULL)
        return;
        
    fprintf(memory_log_file, "At time %d allocated %d bytes for process %d from %d to %d\n",
            time, size, process_id, start_address, end_address);
    fflush(memory_log_file);
    
    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[MEMORY] At time %d allocated %d bytes for process %d from %d to %d\n"ANSI_COLOR_RESET,
            time, size, process_id, start_address, end_address);
}

void mm_log_memory_deallocation(int time, int process_id, int size, int start_address, int end_address)
{
    if (memory_log_file == NULL)
        return;
        
    fprintf(memory_log_file, "At time %d freed %d bytes from process %d from %d to %d\n",
            time, size, process_id, start_address, end_address);
    fflush(memory_log_file);
    
    if (DEBUG)
        printf(ANSI_COLOR_BLUE"[MEMORY] At time %d freed %d bytes from process %d from %d to %d\n"ANSI_COLOR_RESET,
            time, size, process_id, start_address, end_address);
}

void mm_close_memory_log()
{
    if (memory_log_file != NULL)
    {
        fclose(memory_log_file);
        memory_log_file = NULL;
    }
}

bool mm_init(unsigned int memory_size) {
    if (mm != NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Already initialized!\n" ANSI_COLOR_RESET);
        }
        return false;
    }
    
    // Ensure memory size is a power of 2
    if (!is_power_of_2(memory_size)) {
        memory_size = next_power_of_2(memory_size);
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] Adjusted memory size to %u (power of 2)\n" 
                ANSI_COLOR_RESET, memory_size);
        }
    }
    
    mm = malloc(sizeof(memory_manager_t));
    if (!mm) {
        if (DEBUG) {
            perror(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to allocate memory manager" ANSI_COLOR_RESET);
        }
        return false;
    }
    
    // Initialize buddy system
    mm->memory = buddy_new(memory_size);
    if (mm->memory == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize buddy system\n" ANSI_COLOR_RESET);
        }
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize waiting list as min heap
    mm->waiting_list = create_min_heap(MAX_INPUT_PROCESSES, waiting_process_compare);
    if (mm->waiting_list == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize waiting list\n" ANSI_COLOR_RESET);
        }
        buddy_destroy(mm->memory);
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize process ID to offset map
    mm->max_id = 1000; // Assume max 1000 process IDs
    mm->id_to_offset_map = malloc(sizeof(int) * (mm->max_id + 1));
    if (mm->id_to_offset_map == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize ID to offset map\n" ANSI_COLOR_RESET);
        }
        destroy_min_heap(mm->waiting_list);
        buddy_destroy(mm->memory);
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize all offsets to -1 (not allocated)
    for (int i = 0; i <= mm->max_id; i++) {
        mm->id_to_offset_map[i] = -1;
    }
    
    // Initialize ID to size map
    mm->id_to_size_map = malloc(sizeof(size_t) * (mm->max_id + 1));
    if (mm->id_to_size_map == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize ID to size map\n" ANSI_COLOR_RESET);
        }
        free(mm->id_to_offset_map);
        destroy_min_heap(mm->waiting_list);
        buddy_destroy(mm->memory);
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize all sizes to 0 (not allocated)
    for (int i = 0; i <= mm->max_id; i++) {
        mm->id_to_size_map[i] = 0;
    }
    
    // Initialize PID to ID mapping
    mm->max_pid = 10000; // Assume max 10000 PIDs (larger than process IDs)
    mm->pid_to_id_map = malloc(sizeof(int) * (mm->max_pid + 1));
    if (mm->pid_to_id_map == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize PID to ID map\n" ANSI_COLOR_RESET);
        }
        free(mm->id_to_size_map);
        free(mm->id_to_offset_map);
        destroy_min_heap(mm->waiting_list);
        buddy_destroy(mm->memory);
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize all mappings to -1 (no mapping)
    for (int i = 0; i <= mm->max_pid; i++) {
        mm->pid_to_id_map[i] = -1;
    }
    
    // Initialize ID to PID mapping
    mm->id_to_pid_map = malloc(sizeof(int) * (mm->max_id + 1));
    if (mm->id_to_pid_map == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to initialize ID to PID map\n" ANSI_COLOR_RESET);
        }
        free(mm->pid_to_id_map);
        free(mm->id_to_size_map);
        free(mm->id_to_offset_map);
        destroy_min_heap(mm->waiting_list);
        buddy_destroy(mm->memory);
        free(mm);
        mm = NULL;
        return false;
    }
    
    // Initialize all mappings to -1 (no mapping)
    for (int i = 0; i <= mm->max_id; i++) {
        mm->id_to_pid_map[i] = -1;
    }
    
    // Initialize memory log
    mm_initialize_memory_log();
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Initialized with %u memory units\n" ANSI_COLOR_RESET, memory_size);
    }
    
    return true;
}

void mm_destroy() {
    if (mm == NULL) {
        return;
    }
    
    // Clean up buddy system
    buddy_destroy(mm->memory);
    
    // Clean up waiting list
    if (mm->waiting_list != NULL) {
        while (!min_heap_is_empty(mm->waiting_list)) {
            waiting_process_t* process = min_heap_extract_min(mm->waiting_list);
            free(process);
        }
        destroy_min_heap(mm->waiting_list);
    }
    
    // Clean up ID to offset map
    free(mm->id_to_offset_map);
    
    // Clean up ID to size map
    free(mm->id_to_size_map);
    
    // Clean up PID to ID map
    free(mm->pid_to_id_map);
    
    // Clean up ID to PID map
    free(mm->id_to_pid_map);
    
    // Close memory log
    mm_close_memory_log();
    
    // Clean up memory manager
    free(mm);
    mm = NULL;
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Destroyed\n" ANSI_COLOR_RESET);
    }
}

// Map a PID to a process ID
bool mm_map_pid_to_id(int pid, int process_id) {
    if (mm == NULL || pid < 0 || process_id < 0) {
        return false;
    }
    
    // Expand PID map if needed
    if (pid > mm->max_pid) {
        int new_max_pid = pid + 1000; // Add some buffer
        int* new_pid_map = realloc(mm->pid_to_id_map, sizeof(int) * (new_max_pid + 1));
        if (!new_pid_map) {
            if (DEBUG) {
                printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to reallocate PID to ID map\n" ANSI_COLOR_RESET);
            }
            return false;
        }
        
        // Initialize new entries to -1
        for (int i = mm->max_pid + 1; i <= new_max_pid; i++) {
            new_pid_map[i] = -1;
        }
        
        mm->pid_to_id_map = new_pid_map;
        mm->max_pid = new_max_pid;
    }
    
    // Expand ID map if needed
    if (process_id > mm->max_id) {
        int new_max_id = process_id + 100;
        int* new_offset_map = realloc(mm->id_to_offset_map, sizeof(int) * (new_max_id + 1));
        if (!new_offset_map) return false;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_offset_map[i] = -1;
        mm->id_to_offset_map = new_offset_map;
        
        size_t* new_size_map = realloc(mm->id_to_size_map, sizeof(size_t) * (new_max_id + 1));
        if (!new_size_map) return false;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_size_map[i] = 0;
        mm->id_to_size_map = new_size_map;
        
        int* new_id_to_pid_map = realloc(mm->id_to_pid_map, sizeof(int) * (new_max_id + 1));
        if (!new_id_to_pid_map) return false;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_id_to_pid_map[i] = -1;
        mm->id_to_pid_map = new_id_to_pid_map;
        
        mm->max_id = new_max_id;
    }
    
    // Clear any existing mapping for this PID
    int old_process_id = mm->pid_to_id_map[pid];
    if (old_process_id != -1) {
        mm->id_to_pid_map[old_process_id] = -1;
    }
    
    // Clear any existing mapping for this process ID
    int old_pid = mm->id_to_pid_map[process_id];
    if (old_pid != -1) {
        mm->pid_to_id_map[old_pid] = -1;
    }
    
    // Store the bidirectional mapping
    mm->pid_to_id_map[pid] = process_id;
    mm->id_to_pid_map[process_id] = pid;
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Mapped PID %d to process ID %d\n" 
            ANSI_COLOR_RESET, pid, process_id);
    }
    
    return true;
}

// Get process ID from PID (returns -1 if not found)
int mm_get_id_by_pid(int pid) {
    if (mm == NULL || pid < 0 || pid > mm->max_pid) {
        return -1;
    }
    
    return mm->pid_to_id_map[pid];
}

// Implementation for mm_map_id_to_pid
bool mm_map_id_to_pid(int process_id, int pid) {
    if (mm == NULL || process_id < 0 || process_id > mm->max_id || pid < 0) {
        return false;
    }
    
    // Expand PID map if needed
    if (pid > mm->max_pid) {
        int new_max_pid = pid + 1000;
        int* new_pid_map = realloc(mm->pid_to_id_map, sizeof(int) * (new_max_pid + 1));
        if (!new_pid_map) {
            if (DEBUG) {
                printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to reallocate PID to ID map\n" ANSI_COLOR_RESET);
            }
            return false;
        }
        for (int i = mm->max_pid + 1; i <= new_max_pid; i++) {
            new_pid_map[i] = -1;
        }
        mm->pid_to_id_map = new_pid_map;
        mm->max_pid = new_max_pid;
    }
    
    // Clear any existing mapping for this PID
    if (mm->pid_to_id_map[pid] != -1) {
        mm->id_to_pid_map[mm->pid_to_id_map[pid]] = -1;
    }
    
    // Update bidirectional mappings
    mm->pid_to_id_map[pid] = process_id;
    mm->id_to_pid_map[process_id] = pid;
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Mapped process ID %d to PID %d\n" 
            ANSI_COLOR_RESET, process_id, pid);
    }
    
    return true;
}

// Implementation for mm_get_pid_by_id
int mm_get_pid_by_id(int process_id) {
    if (mm == NULL || process_id < 0 || process_id > mm->max_id) {
        return -1;
    }
    return mm->id_to_pid_map[process_id];
}

// Modified version of mm_free to work with PIDs by first converting to process IDs
void mm_free(int pid) {
    if (mm == NULL || pid < 0) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Invalid free request: PID %d\n" ANSI_COLOR_RESET, pid);
        }
        return;
    }
    
    // First check if pid is within bounds
    if (pid > mm->max_pid) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] PID %d exceeds max PID range, assuming no allocation\n" 
                ANSI_COLOR_RESET, pid);
        }
        return;
    }
    
    int process_id = mm_get_id_by_pid(pid);
    if (process_id == -1) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] PID %d has no mapping to a process ID\n" 
                ANSI_COLOR_RESET, pid);
        }
        return;
    }
    
    // Found a mapping to process ID, free by that ID
    mm_free_by_id(process_id);
    
    // Clear the bidirectional mapping
    mm->pid_to_id_map[pid] = -1;
    mm->id_to_pid_map[process_id] = -1;
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Cleared mapping between PID %d and process ID %d\n" 
            ANSI_COLOR_RESET, pid, process_id);
    }
}

// Implementation for mm_check_pid_allocation - properly handles PIDs
bool mm_check_pid_allocation(int pid, int* offset, size_t* size) {
    if (mm == NULL || pid < 0) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Invalid PID %d\n" ANSI_COLOR_RESET, pid);
        }
        return false;
    }
    
    // Check if PID is within bounds
    if (pid > mm->max_pid) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] PID %d exceeds max PID range\n" ANSI_COLOR_RESET, pid);
        }
        return false;
    }

    int process_id = mm_get_id_by_pid(pid);
    if (process_id == -1) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] PID %d is not mapped to any process ID\n" ANSI_COLOR_RESET, pid);
        }
        return false;
    }
    
    // Check if the process ID has memory allocated
    if (process_id <= mm->max_id && mm->id_to_offset_map[process_id] != -1) {
        if (offset) *offset = mm->id_to_offset_map[process_id];
        if (size) *size = mm->id_to_size_map[process_id];
        return true;
    }
    
    if (DEBUG) {
        printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] PID %d (process ID %d) has no memory allocation\n" 
            ANSI_COLOR_RESET, pid, process_id);
    }
    return false;
}

// Keep mm_free_by_id separate for process IDs - rename for clarity
void mm_free_by_id(int process_id) {
    if (mm == NULL || process_id < 0) {
        if (DEBUG) printf(ANSI_COLOR_RED "[MEMORY MANAGER] Attempted to free invalid process ID %d\n" ANSI_COLOR_RESET, process_id);
        return;
    }
    
    // Check if process_id is within bounds
    if (process_id > mm->max_id) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] Process ID %d exceeds max ID range, assuming no allocation\n" 
                ANSI_COLOR_RESET, process_id);
        }
        return;
    }
    
    // Process IDs may be different from PIDs, but in this implementation
    // we use them interchangeably with the ID as the array index
    int offset = mm->id_to_offset_map[process_id];
    if (offset == -1) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] Process ID %d does not have memory allocated\n" ANSI_COLOR_RESET, process_id);
        }
        return;
    }
    
    size_t size = mm->id_to_size_map[process_id];
    buddy_free(mm->memory, offset);
    mm_log_memory_deallocation(get_clk(), process_id, size, offset, offset + size - 1);
    mm->id_to_offset_map[process_id] = -1;
    mm->id_to_size_map[process_id] = 0;
    
    if (DEBUG) {
        printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Freed memory at offset %d for process ID %d\n" ANSI_COLOR_RESET, offset, process_id);
    }
}

int mm_allocate(int process_id, size_t size) {
    if (mm == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Invalid allocation request: mm is NULL\n" ANSI_COLOR_RESET);
        }
        return -1;
    }
    if (process_id < 0) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Invalid allocation request: process_id %d is negative\n" ANSI_COLOR_RESET, process_id);
        }
        return -1;
    }
    
    // Expand ID map if needed
    if (process_id > mm->max_id) {
        int new_max_id = process_id + 100;
        int* new_offset_map = realloc(mm->id_to_offset_map, sizeof(int) * (new_max_id + 1));
        if (!new_offset_map) return -1;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_offset_map[i] = -1;
        mm->id_to_offset_map = new_offset_map;
        
        size_t* new_size_map = realloc(mm->id_to_size_map, sizeof(size_t) * (new_max_id + 1));
        if (!new_size_map) return -1;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_size_map[i] = 0;
        mm->id_to_size_map = new_size_map;
        
        int* new_id_to_pid_map = realloc(mm->id_to_pid_map, sizeof(int) * (new_max_id + 1));
        if (!new_id_to_pid_map) return -1;
        for (int i = mm->max_id + 1; i <= new_max_id; i++) new_id_to_pid_map[i] = -1;
        mm->id_to_pid_map = new_id_to_pid_map;
        
        mm->max_id = new_max_id;
    }
    
    // Check for double allocation
    if (mm->id_to_offset_map[process_id] != -1) {
        if (DEBUG) {
            printf(ANSI_COLOR_YELLOW "[MEMORY MANAGER] Process ID %d already has memory allocated at offset %d, freeing it first\n" 
                ANSI_COLOR_RESET, process_id, mm->id_to_offset_map[process_id]);
        }
        mm_free_by_id(process_id); // Free previous allocation before re-allocating
    }
    
    int offset = buddy_alloc(mm->memory, size);
    if (offset != -1) {
        mm->id_to_offset_map[process_id] = offset;
        mm->id_to_size_map[process_id] = size;
        mm_log_memory_allocation(get_clk(), process_id, size, offset, offset + size - 1);
        if (DEBUG) printf(ANSI_COLOR_GREEN "[MEMORY MANAGER] Allocated %zu units for process ID %d at offset %d\n" ANSI_COLOR_RESET, size, process_id, offset);
    } else {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to allocate %zu units for process ID %d (no suitable block found)\n" ANSI_COLOR_RESET, size, process_id);
            mm_dump_memory();
        }
    }
    return offset;
}

int mm_add_to_waiting_list(processParameters* process_params) {
    if (mm == NULL || process_params == NULL) {
        return -1;
    }
    waiting_process_t* waiting_process = malloc(sizeof(waiting_process_t));
    if (!waiting_process) {
        if (DEBUG) {
            perror(ANSI_COLOR_RED "[MEMORY MANAGER] Failed to allocate waiting process" ANSI_COLOR_RESET);
        }
        return -1;
    }
    // Copy process parameters, but clear PID (not known yet)
    memcpy(&waiting_process->params, process_params, sizeof(processParameters));
    waiting_process->params.pid = 0; // PID not known until fork
    waiting_process->size = process_params->memsize;
    min_heap_insert(mm->waiting_list, waiting_process);
    if (DEBUG) {
        printf(ANSI_COLOR_CYAN "[MEMORY MANAGER] Added process ID %d to waiting list (size: %zu, position: %d)\n"
            ANSI_COLOR_RESET, process_params->id, waiting_process->size, mm->waiting_list->size);
    }
    return mm->waiting_list->size;
}

processParameters* mm_get_next_allocatable_process() {
    if (mm == NULL || min_heap_is_empty(mm->waiting_list)) {
        return NULL;
    }
    int waiting_count = mm->waiting_list->size;
    waiting_process_t** waiting_copy = malloc(waiting_count * sizeof(waiting_process_t*));
    if (!waiting_copy) return NULL;
    for (int i = 0; i < waiting_count; i++) waiting_copy[i] = min_heap_extract_min(mm->waiting_list);
    processParameters* result = NULL;
    int allocated_index = -1;
    for (int i = 0; i < waiting_count; i++) {
        if (waiting_copy[i]) {
            // Try to allocate memory (simulate, since PID not known yet)
            int offset = buddy_alloc(mm->memory, waiting_copy[i]->size);
            if (offset != -1) {
                // Do NOT record allocation yet, just return the process parameters
                result = malloc(sizeof(processParameters));
                if (!result) {
                    buddy_free(mm->memory, offset);
                    continue;
                }
                memcpy(result, &waiting_copy[i]->params, sizeof(processParameters));
                // Free buddy block, will be re-allocated after fork with real PID
                buddy_free(mm->memory, offset);
                allocated_index = i;
                break;
            }
        }
    }
    for (int i = 0; i < waiting_count; i++) {
        if (i != allocated_index && waiting_copy[i]) min_heap_insert(mm->waiting_list, waiting_copy[i]);
        else if (i == allocated_index) free(waiting_copy[i]);
    }
    free(waiting_copy);
    return result;
}

bool mm_has_waiting_processes() {
    return mm != NULL && !min_heap_is_empty(mm->waiting_list);
}

int mm_get_waiting_count() {
    if (mm == NULL || mm->waiting_list == NULL) {
        return 0;
    }
    return mm->waiting_list->size;
}

void mm_dump_memory() {
    if (mm == NULL || mm->memory == NULL) {
        printf(ANSI_COLOR_RED "[MEMORY MANAGER] Not initialized\n" ANSI_COLOR_RESET);
        return;
    }
    
    printf(ANSI_COLOR_CYAN "[MEMORY MANAGER] Waiting list (%d processes):\n" ANSI_COLOR_RESET,
        mm->waiting_list->size);
    
    // Show waiting processes if any
    if (!min_heap_is_empty(mm->waiting_list)) {
        waiting_process_t* top = min_heap_get_min(mm->waiting_list);
        printf(ANSI_COLOR_CYAN "[MEMORY MANAGER] First waiting process: ID %d, size %zu\n" ANSI_COLOR_RESET,
            top->params.id, top->size);
    }
    
    // Report total memory stats
    size_t total_memory = 0;
    mm_get_stats(&total_memory, NULL, NULL);
    printf(ANSI_COLOR_CYAN "[MEMORY MANAGER] Total memory: %zu units\n" ANSI_COLOR_RESET, total_memory);
}

void mm_get_stats(size_t* total_memory, size_t* free_memory, size_t* largest_block) {
    if (mm == NULL || mm->memory == NULL) {
        if (total_memory) *total_memory = 0;
        if (free_memory) *free_memory = 0;
        if (largest_block) *largest_block = 0;
        return;
    }
    
    // For now, we'll just set the values we know
    if (total_memory) {
        *total_memory = buddy_get_size(mm->memory);
    }
    
    // Note: Getting free memory and largest block would require
    // additional functionality in the buddy system that we don't have yet
    if (free_memory) *free_memory = 0;  // Placeholder
    if (largest_block) *largest_block = 0;  // Placeholder
}

// Implementation for mm_allocate_by_id
int mm_allocate_by_id(int process_id, size_t size) {
    if (mm == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[MEMORY MANAGER] Invalid allocation request: mm is NULL\n" ANSI_COLOR_RESET);
        }
        return -1;
    }
    
    // Same implementation as mm_allocate since we're using process_id directly
    return mm_allocate(process_id, size);
}


// Implementation for mm_check_id_allocation
bool mm_check_id_allocation(int process_id, int* offset, size_t* size) {
    if (mm == NULL || process_id < 0 || process_id > mm->max_id) return false;
    if (mm->id_to_offset_map[process_id] != -1) {
        if (offset) *offset = mm->id_to_offset_map[process_id];
        if (size) *size = mm->id_to_size_map[process_id];
        return true;
    }
    return false;
}

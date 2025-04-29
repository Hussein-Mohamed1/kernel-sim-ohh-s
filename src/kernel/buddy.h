#define TOTAL_MEMORY 1024  
#define MIN_BLOCK 32

typedef struct {
    int size;
    int is_free;
    int pid;
} Block;



void init_buddy();
int allocate_memory(int pid, int size);
int split_block(int index);
int find_free_index();
void free_memory(int pid);
void merge_buddies(int index);
void destruct_buddy();
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "buddy.h"
#include "colors.h"
#include "../data_structures/binary_tree.h"

/*
 * Workflow of memory allocation:
    * 1. Check if the requested size is a power of 2.
    * 2. If not, round it up to the next power of 2.
    * 3. Traverse the binary tree to find a suitable block.
    * 4. If a suitable block is found, mark it as allocated.
    * 5. Update the longest free block size for the parent nodes.
    * 6. Return the offset of the allocated block.
 * 7. If no suitable block is found, return -1.
*/

/*
 * Workflow of memory deallocation:
    * 1. Find the node representing the allocated block using its offset.
    * 2. Mark the block as free.
    * 3. Update the longest free block size for the parent nodes.
    * 4. Check if the buddy block can be merged with this block.
    * 5. If they can be merged, update the size and mark it as free.
    * 6. Update the longest free block size for the parent nodes.
    * 7. Continue merging until no more merges are possible.
 * 8. Return the updated longest free block size.
*/

/*
 * Workflow of buddy system:
    * 1. Initialize the buddy system with a given size.
    * 2. Create a binary tree to manage memory blocks.
    * 3. Each node in the tree represents a block of memory.
    * 4. The size of each block is a power of 2.
    * 5. The longest free block size is updated during allocation and deallocation.
    * 6. The buddy system allows for efficient memory allocation and deallocation.
*/

#define max(a, b) (((a)>(b))?(a):(b))

typedef struct buddy_node_data {
    size_t size;        
    size_t longest_free;
    int offset;       
    bool is_allocated;
    int pid;        
} buddy_node_data_t;

struct buddy {
    size_t size;        
    binary_tree_t* tree;   
};

static void *b_malloc(size_t size)
{
    void *tmp = malloc(size);
    if (tmp == NULL) {
        fprintf(stderr, "my_malloc: not enough memory, quit\n");
        exit(EXIT_FAILURE);
    }
    return tmp;
}

static inline bool is_power_of_2(int index)
{
    return !(index & (index - 1));
}

static inline unsigned next_power_of_2(unsigned x)
{
    if (is_power_of_2(x)) {
        return x;
    }

    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    
    return x;
}


// Compare function for buddy nodes based on offset
static int buddy_node_compare(const void* a, const void* b)
{
    buddy_node_data_t* node_a = (buddy_node_data_t*)a;
    buddy_node_data_t* node_b = (buddy_node_data_t*)b;
    
    return node_a->offset - node_b->offset;
}

// Update the longest_free value of a node based on its children
static void update_longest_free(bt_node_t* node)
{
    if (!node) return;
    
    buddy_node_data_t* data = (buddy_node_data_t*)node->data;
    
    if (!node->left && !node->right) {
        // Leaf node
        data->longest_free = data->is_allocated ? 0 : data->size;
        return;
    }
    
    buddy_node_data_t* left_data = node->left ? (buddy_node_data_t*)node->left->data : NULL;
    buddy_node_data_t* right_data = node->right ? (buddy_node_data_t*)node->right->data : NULL;
    
    size_t left_longest = left_data ? left_data->longest_free : 0;
    size_t right_longest = right_data ? right_data->longest_free : 0;
    
    if (!data->is_allocated) {
        // If this block is free, it could be the longest
        data->longest_free = data->size;
    } else {
        // Otherwise, the longest is the max of children
        data->longest_free = max(left_longest, right_longest);
    }
}

// Recursively build the buddy tree
static bt_node_t* build_buddy_tree(binary_tree_t* tree, size_t size, int offset)
{
    buddy_node_data_t node_data;
    node_data.size = size;
    node_data.longest_free = size;
    node_data.offset = offset;
    node_data.is_allocated = false;
    node_data.pid = -1;
    
    bt_node_t* node = bt_create_node(tree, &node_data);
    
    if (size > 1) {
        size_t child_size = size / 2;
        
        node->left = build_buddy_tree(tree, child_size, offset);
        if (node->left) node->left->parent = node;
        
        node->right = build_buddy_tree(tree, child_size, offset + child_size);
        if (node->right) node->right->parent = node;
    }
    
    return node;
}

struct buddy *buddy_new(unsigned num_of_fragments)
{
    if (num_of_fragments < 1 || !is_power_of_2(num_of_fragments)) {
        return NULL;
    }
    
    struct buddy *self = (struct buddy*)b_malloc(sizeof(struct buddy));
    self->size = num_of_fragments;
    
    // Create binary tree for buddy system
    self->tree = bt_create(sizeof(buddy_node_data_t), buddy_node_compare);
    
    // Build the initial tree
    self->tree->root = build_buddy_tree(self->tree, num_of_fragments, 0);
    
    return self;
}

void buddy_destroy(struct buddy *self)
{
    if (self) {
        if (self->tree) {
            bt_destroy(self->tree);
        }
        free(self);
    }
}

// Add this new function to expose the total memory size
size_t buddy_get_size(struct buddy *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->size;
}

// Find a suitable block in the tree for allocation
static bt_node_t* find_suitable_block(bt_node_t* node, size_t size)
{
    if (!node) return NULL;
    
    buddy_node_data_t* data = (buddy_node_data_t*)node->data;
    
    // If this node is too small or already allocated, return NULL
    if (data->longest_free < size) {
        return NULL;
    }
    
    // If this node is exactly the right size and free, return it
    if (data->size == size && !data->is_allocated) {
        return node;
    }
    
    // Try left subtree first
    bt_node_t* left_result = NULL;
    if (node->left) {
        buddy_node_data_t* left_data = (buddy_node_data_t*)node->left->data;
        if (left_data->longest_free >= size) {
            left_result = find_suitable_block(node->left, size);
        }
    }
    
    if (left_result) {
        return left_result;
    }
    
    // Try right subtree if left didn't work
    return find_suitable_block(node->right, size);
}

int buddy_alloc(struct buddy *self, size_t size)
{
    if (self == NULL) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[BUDDY] Cannot allocate: buddy system is NULL\n" ANSI_COLOR_RESET);
        }
        return -1;
    }
    
    if (self->size < size) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[BUDDY] Cannot allocate: requested size %zu exceeds total memory %zu\n" 
                ANSI_COLOR_RESET, size, self->size);
        }
        return -1;
    }
    
    // Ensure minimum size is at least 1
    if (size < 1) size = 1;
    
    size_t original_size = size;
    size = next_power_of_2(size);
    
    if (DEBUG) {
        printf(ANSI_COLOR_CYAN "[BUDDY] Attempting to allocate %zu units (rounded from %zu to power of 2)\n" 
            ANSI_COLOR_RESET, size, original_size);
    }
    
    bt_node_t* node = find_suitable_block(self->tree->root, size);
    if (!node) {
        if (DEBUG) {
            printf(ANSI_COLOR_RED "[BUDDY] Failed to find suitable block for size %zu\n" 
                ANSI_COLOR_RESET, size);
        }
        return -1;  // No suitable block found
    }
    
    buddy_node_data_t* data = (buddy_node_data_t*)node->data;
    
    // Mark this node as allocated
    data->is_allocated = true;
    data->longest_free = 0;
    
    // Update longest_free values up the tree
    bt_node_t* current = node->parent;
    while (current) {
        update_longest_free(current);
        current = current->parent;
    }
    
    if (DEBUG) {
        printf(ANSI_COLOR_CYAN "[BUDDY] Successfully allocated block at offset %d, size %zu\n" 
            ANSI_COLOR_RESET, data->offset, data->size);
    }
    
    return data->offset;
}

void buddy_free(struct buddy *self, int offset)
{
    if (self == NULL || offset < 0 || offset > self->size) {
        return;
    }
    
    // Find the node representing this allocation
    bt_node_t* node = self->tree->root;
    size_t node_size = self->size;
    
    while (node) {
        buddy_node_data_t* data = (buddy_node_data_t*)node->data;
        
        if (data->offset == offset && data->is_allocated) {
            // Found the node, mark it as free
            data->is_allocated = false;
            data->pid = -1;
            data->longest_free = data->size;
            break;
        }
        
        node_size /= 2;
        if (offset < data->offset + node_size) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    
    if (!node) {
        return;  // Node not found
    }
    
    // Try to merge with buddy if possible
    bt_node_t* current = node->parent;
    while (current) {
        buddy_node_data_t* data = (buddy_node_data_t*)current->data;
        buddy_node_data_t* left_data = (buddy_node_data_t*)current->left->data;
        buddy_node_data_t* right_data = (buddy_node_data_t*)current->right->data;
        
        // Check if both children are free
        if (!left_data->is_allocated && !right_data->is_allocated && 
            left_data->longest_free == left_data->size && 
            right_data->longest_free == right_data->size) {
            data->longest_free = data->size;
        } else {
            data->longest_free = max(left_data->longest_free, right_data->longest_free);
        }
        
        current = current->parent;
    }
}

static void print_node_data(void* data)
{
    buddy_node_data_t* node_data = (buddy_node_data_t*)data;
    printf("Offset: %d, Size: %zu, Longest: %zu, Allocated: %d\n", 
           node_data->offset, node_data->size, node_data->longest_free, node_data->is_allocated);
}

void buddy_dump(struct buddy *self)
{
    if (!self) {
        printf("Buddy System is NULL!\n");
        return;
    }

    printf("Buddy System Dump:\n");
    printf("Total size: %zu\n", self->size);
    
    if (!self->tree) {
        printf("Tree is NULL!\n");
        return;
    }
    
    if (!self->tree->root) {
        printf("Tree root is NULL!\n");
        return;
    }

    printf("Tree structure:\n");
    
    // Print the root node details first
    buddy_node_data_t* root_data = (buddy_node_data_t*)self->tree->root->data;
    printf("Root node - Offset: %d, Size: %zu, Longest free: %zu, Allocated: %d\n", 
           root_data->offset, root_data->size, root_data->longest_free, root_data->is_allocated);
    
    // Print tree in level order
    int height = bt_height(self->tree->root);
    printf("Tree height: %d\n", height);
    
    for (int level = 0; level <= height; level++) {
        printf("Level %d: ", level);
        bt_preorder_traverse(self->tree, self->tree->root, print_node_data);
        printf("\n");
    }
}
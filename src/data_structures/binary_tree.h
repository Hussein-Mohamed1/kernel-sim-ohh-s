#ifndef BINARY_TREE_H
#define BINARY_TREE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Generic binary tree node structure
 */
typedef struct bt_node {
    void* data;              /* Pointer to node data */
    struct bt_node* left;    /* Left child */
    struct bt_node* right;   /* Right child */
    struct bt_node* parent;  /* Parent node */
} bt_node_t;

/**
 * Binary tree structure
 */
typedef struct binary_tree {
    bt_node_t* root;         /* Root of the tree */
    size_t size;             /* Number of nodes in the tree */
    size_t data_size;        /* Size of data stored in each node */
    
    /* Function to compare two nodes (returns 0 if equal, <0 if a<b, >0 if a>b) */
    int (*compare)(const void* a, const void* b);
} binary_tree_t;

/* Create a new binary tree */
binary_tree_t* bt_create(size_t data_size, int (*compare)(const void* a, const void* b));

/* Create a new node with the given data */
bt_node_t* bt_create_node(binary_tree_t* tree, void* data);

/* Insert a node into the tree */
bt_node_t* bt_insert(binary_tree_t* tree, void* data);

/* Find a node with matching data */
bt_node_t* bt_find(binary_tree_t* tree, void* data);

/* Calculate the height of a node (distance to leaf) */
int bt_height(bt_node_t* node);

/* Get the level of a node (distance from root) */
int bt_level(bt_node_t* node);

/* Check if node is a leaf (no children) */
bool bt_is_leaf(bt_node_t* node);

/* Remove a node with matching data */
bool bt_remove(binary_tree_t* tree, void* data);

/* Free all memory used by the tree */
void bt_destroy(binary_tree_t* tree);

/* Tree traversal functions */
void bt_inorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data));
void bt_preorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data));
void bt_postorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data));

/* Helper functions for binary tree operations */
bt_node_t* bt_get_left_child(bt_node_t* node);
bt_node_t* bt_get_right_child(bt_node_t* node);
bt_node_t* bt_get_parent(bt_node_t* node);
bool bt_is_internal(bt_node_t* node);
bool bt_is_external(bt_node_t* node);

#endif /* BINARY_TREE_H */

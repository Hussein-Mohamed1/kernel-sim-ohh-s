#include "binary_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Create a new binary tree */
binary_tree_t* bt_create(size_t data_size, int (*compare)(const void* a, const void* b)) {
    binary_tree_t* tree = (binary_tree_t*)malloc(sizeof(binary_tree_t));
    if (!tree) {
        return NULL;
    }
    
    tree->root = NULL;
    tree->size = 0;
    tree->data_size = data_size;
    tree->compare = compare;
    
    return tree;
}

/* Create a new node with the given data */
bt_node_t* bt_create_node(binary_tree_t* tree, void* data) {
    bt_node_t* node = (bt_node_t*)malloc(sizeof(bt_node_t));
    if (!node) {
        return NULL;
    }
    
    node->data = malloc(tree->data_size);
    if (!node->data) {
        free(node);
        return NULL;
    }
    
    memcpy(node->data, data, tree->data_size);
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    
    return node;
}

/* Helper function to insert node based on binary search tree property */
static bt_node_t* _insert_recursive(binary_tree_t* tree, bt_node_t* root, bt_node_t* new_node) {
    if (!root) {
        return new_node;
    }
    
    int cmp = tree->compare(new_node->data, root->data);
    
    if (cmp < 0) {
        root->left = _insert_recursive(tree, root->left, new_node);
        if (root->left) {
            root->left->parent = root;
        }
    } else {
        root->right = _insert_recursive(tree, root->right, new_node);
        if (root->right) {
            root->right->parent = root;
        }
    }
    
    return root;
}

/* Insert a node into the tree */
bt_node_t* bt_insert(binary_tree_t* tree, void* data) {
    bt_node_t* new_node = bt_create_node(tree, data);
    if (!new_node) {
        return NULL;
    }
    
    if (!tree->root) {
        tree->root = new_node;
    } else {
        _insert_recursive(tree, tree->root, new_node);
    }
    
    tree->size++;
    return new_node;
}

/* Helper function to find a node recursively */
static bt_node_t* _find_recursive(binary_tree_t* tree, bt_node_t* root, void* data) {
    if (!root) {
        return NULL;
    }
    
    int cmp = tree->compare(data, root->data);
    
    if (cmp == 0) {
        return root;
    } else if (cmp < 0) {
        return _find_recursive(tree, root->left, data);
    } else {
        return _find_recursive(tree, root->right, data);
    }
}

/* Find a node with matching data */
bt_node_t* bt_find(binary_tree_t* tree, void* data) {
    return _find_recursive(tree, tree->root, data);
}

/* Calculate the height of a node */
int bt_height(bt_node_t* node) {
    if (!node) {
        return -1;
    }
    
    int left_height = bt_height(node->left);
    int right_height = bt_height(node->right);
    
    return (left_height > right_height ? left_height : right_height) + 1;
}

/* Get the level of a node */
int bt_level(bt_node_t* node) {
    int level = 0;
    bt_node_t* current = node;
    
    while (current && current->parent) {
        level++;
        current = current->parent;
    }
    
    return level;
}

/* Check if node is a leaf */
bool bt_is_leaf(bt_node_t* node) {
    return node && !node->left && !node->right;
}

/* Helper function to free a node and its children recursively */
static void _free_recursive(bt_node_t* node) {
    if (!node) {
        return;
    }
    
    _free_recursive(node->left);
    _free_recursive(node->right);
    
    free(node->data);
    free(node);
}

/* Free all memory used by the tree */
void bt_destroy(binary_tree_t* tree) {
    if (!tree) {
        return;
    }
    
    _free_recursive(tree->root);
    free(tree);
}

/* Find the minimum value node in a subtree */
static bt_node_t* _find_min(bt_node_t* node) {
    bt_node_t* current = node;
    
    while (current && current->left) {
        current = current->left;
    }
    
    return current;
}

/* Helper function for node removal */
static bt_node_t* _remove_recursive(binary_tree_t* tree, bt_node_t* root, void* data, bool* success) {
    if (!root) {
        *success = false;
        return NULL;
    }
    
    int cmp = tree->compare(data, root->data);
    
    if (cmp < 0) {
        root->left = _remove_recursive(tree, root->left, data, success);
        if (root->left) {
            root->left->parent = root;
        }
    } else if (cmp > 0) {
        root->right = _remove_recursive(tree, root->right, data, success);
        if (root->right) {
            root->right->parent = root;
        }
    } else {
        /* Node found, perform removal */
        *success = true;
        
        /* Case 1: No children or only one child */
        if (!root->left) {
            bt_node_t* temp = root->right;
            free(root->data);
            free(root);
            return temp;
        } else if (!root->right) {
            bt_node_t* temp = root->left;
            free(root->data);
            free(root);
            return temp;
        }
        
        /* Case 2: Two children */
        bt_node_t* successor = _find_min(root->right);
        memcpy(root->data, successor->data, tree->data_size);
        root->right = _remove_recursive(tree, root->right, successor->data, success);
        if (root->right) {
            root->right->parent = root;
        }
    }
    
    return root;
}

/* Remove a node with matching data */
bool bt_remove(binary_tree_t* tree, void* data) {
    bool success = false;
    
    tree->root = _remove_recursive(tree, tree->root, data, &success);
    
    if (success) {
        tree->size--;
    }
    
    return success;
}

/* Tree traversal functions */
void bt_inorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data)) {
    if (!node) {
        return;
    }
    
    bt_inorder_traverse(tree, node->left, visit);
    visit(node->data);
    bt_inorder_traverse(tree, node->right, visit);
}

void bt_preorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data)) {
    if (!node) {
        return;
    }
    
    visit(node->data);
    bt_preorder_traverse(tree, node->left, visit);
    bt_preorder_traverse(tree, node->right, visit);
}

void bt_postorder_traverse(binary_tree_t* tree, bt_node_t* node, void (*visit)(void* data)) {
    if (!node) {
        return;
    }
    
    bt_postorder_traverse(tree, node->left, visit);
    bt_postorder_traverse(tree, node->right, visit);
    visit(node->data);
}

/* Helper functions */
bt_node_t* bt_get_left_child(bt_node_t* node) {
    return node ? node->left : NULL;
}

bt_node_t* bt_get_right_child(bt_node_t* node) {
    return node ? node->right : NULL;
}

bt_node_t* bt_get_parent(bt_node_t* node) {
    return node ? node->parent : NULL;
}

bool bt_is_internal(bt_node_t* node) {
    return node && (node->left || node->right);
}

bool bt_is_external(bt_node_t* node) {
    return node && !node->left && !node->right;
}

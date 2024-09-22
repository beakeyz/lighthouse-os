#ifndef __ANIVA_TREE__
#define __ANIVA_TREE__

#include "libk/flow/error.h"

struct binary_tree;
struct binary_tree_node;

/*
 * Function description for a binary tree comperator
 *
 * This gives the user of this datastructure some control over how the tree gets constructed
 */
typedef bool (*f_bin_tree_comperator_t)(struct binary_tree_node* current, void* data, struct binary_tree_node*** pTarget);

typedef struct binary_tree {
    u32 m_depth;
    u32 m_node_count;

    /* Node comperator */
    f_bin_tree_comperator_t f_comperator;

    /* Root node */
    struct binary_tree_node* m_root;
} binary_tree_t;

typedef struct binary_tree_node {
    void* m_data;
    struct binary_tree_node* m_left;
    struct binary_tree_node* m_right;
} binary_tree_node_t;

binary_tree_t* create_binary_tree(f_bin_tree_comperator_t comperator);
void destroy_binary_tree(binary_tree_t* tree);

kerror_t binary_tree_insert(binary_tree_t* tree, void* data);
kerror_t binary_tree_remove(binary_tree_t* tree, void* data);

kerror_t binary_tree_find(binary_tree_t* tree, void* target, void** pData);

#endif // !__ANIVA_TREE__

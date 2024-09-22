#ifndef __ANIVA_TREE__
#define __ANIVA_TREE__

#include "libk/flow/error.h"

struct binary_tree;
struct binary_tree_node;

typedef struct binary_tree {
    u64 m_node_count;
    u32 m_left_bias;
    u32 m_right_bias;

    /* Root node */
    struct binary_tree_node* m_root;
} binary_tree_t;

typedef struct binary_tree_node {
    void* m_data;
    u64 id;
    struct binary_tree_node* m_left;
    struct binary_tree_node* m_right;
} binary_tree_node_t;

binary_tree_t* create_binary_tree();
void destroy_binary_tree(binary_tree_t* tree);

kerror_t binary_tree_insert(binary_tree_t* tree, u64 id, void* data);
kerror_t binary_tree_remove(binary_tree_t* tree, u64 id);

kerror_t binary_tree_find(binary_tree_t* tree, u64 id, void** pData);

#endif // !__ANIVA_TREE__

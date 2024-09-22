#include "bin_tree.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <libk/string.h>

binary_tree_t* create_binary_tree()
{
    binary_tree_t* tree;

    tree = kmalloc(sizeof(*tree));

    if (!tree)
        return nullptr;

    memset(tree, 0, sizeof(*tree));

    return tree;
}

void destroy_binary_tree(binary_tree_t* tree)
{
    // TODO:
    kernel_panic("TODO: destroy_binary_tree");
}

static binary_tree_node_t* create_btree_node(u64 id, void* data)
{
    binary_tree_node_t* node = kmalloc(sizeof(*node));

    if (!node)
        return nullptr;

    node->m_data = data;
    node->id = id;
    node->m_left = nullptr;
    node->m_right = nullptr;

    return node;
}

static void destroy_btree_node(binary_tree_node_t* node)
{
    kfree(node);
}

static void __btree_comperator(binary_tree_node_t* current, u64* left_bias, u64* right_bias, u64 id, binary_tree_node_t*** pTarget)
{
    if (!pTarget)
        return;

    *pTarget = NULL;

    if (!current)
        return;

    /* Can't have duplicate ids in the tree */
    if (current->id == id)
        return;

    if (id > current->id) {
        *pTarget = &current->m_left;
        if (left_bias)
            *left_bias = *left_bias + 1;
    } else {
        *pTarget = &current->m_right;
        if (right_bias)
            *right_bias = *right_bias + 1;
    }
}

kerror_t binary_tree_insert(binary_tree_t* tree, u64 id, void* data)
{
    u64 left_bias = 0, right_bias = 0;
    binary_tree_node_t* walker;
    binary_tree_node_t** target;
    binary_tree_node_t* new_node;

    new_node = create_btree_node(id, data);

    /* Firstly, check if we can put this node in the root, if we can thats great! */
    if (!tree->m_root) {
        tree->m_root = new_node;
        return 0;
    }

    target = nullptr;
    walker = tree->m_root;

    do {
        /* Run the comperator on the current node */
        __btree_comperator(walker, &left_bias, &right_bias, id, &target);

        if (!target)
            break;

        walker = *target;
    } while (walker);

    /* Somehow we could not find a place for this node? */
    if (!target) {
        destroy_btree_node(new_node);
        return -KERR_NOT_FOUND;
    }

    /* Add to the node count */
    tree->m_node_count++;

    if (right_bias > left_bias)
        /* We've had to move more to the right to insert this node */
        tree->m_right_bias++;
    else if (left_bias > right_bias)
        /* We've had to move more to the left to insert this node */
        tree->m_left_bias++;

    /* Put the node at the target */
    *target = new_node;

    return 0;
}

kerror_t binary_tree_remove(binary_tree_t* tree, u64 id)
{
    kernel_panic("TODO: binary_tree_remove");
}

kerror_t binary_tree_find(binary_tree_t* tree, u64 id, void** pData)
{
    binary_tree_node_t* walker;
    binary_tree_node_t** target;

    if (!tree)
        return -KERR_INVAL;

    target = nullptr;
    walker = tree->m_root;

    do {
        if (walker->id == id)
            break;

        /*
         * Run the comperator on the current node
         * to see where we need to go next
         */
        __btree_comperator(walker, NULL, NULL, id, &target);

        if (!target)
            break;

        walker = *target;
        /* We know we can't loop further than the max depth of the tree, so check for safety */
    } while (walker);

    /* We reached the end of the tree =( */
    if (!walker)
        return -KERR_NOT_FOUND;

    /* Export the data */
    if (pData)
        *pData = walker->m_data;

    return 0;
}

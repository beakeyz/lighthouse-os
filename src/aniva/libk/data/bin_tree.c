#include "bin_tree.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <libk/string.h>

binary_tree_t* create_binary_tree(f_bin_tree_comperator_t comperator)
{
    binary_tree_t* tree;

    if (!comperator)
        return nullptr;

    tree = kmalloc(sizeof(*tree));

    if (!tree)
        return nullptr;

    memset(tree, 0, sizeof(*tree));

    tree->m_depth = 0;
    tree->m_node_count = 0;
    tree->f_comperator = comperator;

    return tree;
}

void destroy_binary_tree(binary_tree_t* tree)
{
    // TODO:
    kernel_panic("TODO: destroy_binary_tree");
}

static binary_tree_node_t* create_btree_node(void* data)
{
    binary_tree_node_t* node = kmalloc(sizeof(*node));

    if (!node)
        return nullptr;

    node->m_data = data;
    node->m_left = nullptr;
    node->m_right = nullptr;

    return node;
}

static void destroy_btree_node(binary_tree_node_t* node)
{
    kfree(node);
}

kerror_t binary_tree_insert(binary_tree_t* tree, void* data)
{
    u32 c_depth;
    binary_tree_node_t* walker;
    binary_tree_node_t** target;
    binary_tree_node_t* new_node;

    new_node = create_btree_node(data);

    /* Firstly, check if we can put this node in the root, if we can thats great! */
    if (!tree->m_root) {
        tree->m_depth = 1;
        tree->m_root = new_node;
        return 0;
    }

    c_depth = 0;
    target = nullptr;
    walker = tree->m_root;

    do {
        /* Run the comperator on the current node */
        tree->f_comperator(walker, data, &target);

        if (!target)
            break;

        walker = *target;
        /* We know we can't loop further than the max depth of the tree, so check for safety */
    } while (walker && c_depth++ <= tree->m_depth);

    /* Somehow we could not find a place for this node? */
    if (!target) {
        destroy_btree_node(new_node);
        return -KERR_NOT_FOUND;
    }

    if (c_depth > tree->m_depth)
        tree->m_depth = c_depth;

    /* Put the node at the target */
    *target = new_node;

    return 0;
}

kerror_t binary_tree_remove(binary_tree_t* tree, void* data)
{
    kernel_panic("TODO: binary_tree_remove");
}

kerror_t binary_tree_find(binary_tree_t* tree, void* target_data, void** pData)
{
    u32 c_depth;
    binary_tree_node_t* walker;
    binary_tree_node_t** target;

    if (!tree)
        return -KERR_INVAL;

    c_depth = 0;
    target = nullptr;
    walker = tree->m_root;

    do {
        if (walker->m_data == target_data)
            break;

        /*
         * Run the comperator on the current node
         * to see where we need to go next
         */
        tree->f_comperator(walker, target_data, &target);

        if (!target)
            break;

        walker = *target;
        /* We know we can't loop further than the max depth of the tree, so check for safety */
    } while (walker && c_depth++ <= tree->m_depth);

    /* We reached the end of the tree =( */
    if (!walker)
        return -KERR_NOT_FOUND;

    /* Export the data */
    if (pData)
        *pData = walker->m_data;

    return 0;
}

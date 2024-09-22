#include "abtree.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "sys/types.h"
#include <libk/string.h>

/*
 * A single key-value pair inside an abt node
 */
typedef struct abt_entry {
    u64 key;
    void* data;
} abt_entry_t;

typedef struct abt_node {
    u32 nr_entries;
    /* Array of the abt entries of this node. It has a capacity of b-1 */
    abt_entry_t** vec_entries;
    /* Array of children nodes. It has a capacity of b */
    struct abt_node** vec_children;
} abt_node_t;

static int __abt_node_split(ab_tree_t* tree, abt_node_t** pNode, abt_entry_t* new_entry);

static abt_node_t* __create_abt_node(ab_tree_t* tree)
{
    abt_node_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->nr_entries = 0;
    ret->vec_children = kmalloc(sizeof(abt_node_t*) * tree->b);

    if (!ret->vec_children)
        goto free_and_exit;

    ret->vec_entries = kmalloc(sizeof(abt_entry_t*) * (tree->b - 1));

    if (!ret->vec_entries)
        goto free_and_exit;

    /* Clear the vectors */
    memset(ret->vec_children, 0, sizeof(abt_node_t*) * tree->b);
    memset(ret->vec_entries, 0, sizeof(abt_entry_t*) * (tree->b - 1));

    return ret;

free_and_exit:

    if (ret->vec_children)
        kfree(ret->vec_children);

    if (ret->vec_entries)
        kfree(ret->vec_entries);

    kfree(ret);
    return nullptr;
}

static void __destroy_abt_node(abt_node_t* node)
{
    /* Free the nodes bois */
    kfree(node->vec_entries);
    kfree(node->vec_children);

    /* Free the node itself */
    kfree(node);
}

static inline bool __abt_node_can_insert(ab_tree_t* tree, abt_node_t* node)
{
    return node->nr_entries < (tree->b - 1);
}

/*!
 * @brief: Tries to insert a key into a node
 */
static int __abt_node_insert_entry(ab_tree_t* tree, abt_node_t** pNode, abt_entry_t* entry)
{
    u32 i;
    abt_node_t* node;
    abt_entry_t* c_entry;

    if (!pNode)
        return -KERR_INVAL;

    node = *pNode;

    if (!__abt_node_can_insert(tree, node))
        return __abt_node_split(tree, pNode, entry);

    for (i = 0; i < node->nr_entries; i++) {
        c_entry = node->vec_entries[i];

        /* This would be kinda bad lol */
        if (!c_entry)
            break;

        /* Already have this key */
        if (c_entry->key == entry->key)
            return -KERR_DUPLICATE;

        /* Skip lower keys */
        if (c_entry->key > entry->key)
            break;
    }

    /* If there is a leaf here, just put the thing in */
    if (!node->vec_children[i]) {

        /* Increment this */
        node->nr_entries++;

        /* Place the bitch */
        for (; i < node->nr_entries; i++) {
            /* Switch the entries */
            node->vec_entries[i] = (abt_entry_t*)((u64)node->vec_entries[i] ^ (u64)entry);
            entry = (abt_entry_t*)((u64)entry ^ (u64)node->vec_entries[i]);
            node->vec_entries[i] = (abt_entry_t*)((u64)node->vec_entries[i] ^ (u64)entry);
        }
        /* Assert that we end with an empty entry */
        ASSERT(!entry);
    } else
        return __abt_node_insert_entry(tree, &node->vec_children[i], entry);
    return 0;
}

/*!
 * @brief: Splits a node to satisfy the a-b constraint
 */
static int __abt_node_split(ab_tree_t* tree, abt_node_t** pNode, abt_entry_t* new_entry)
{
    return 0;
}

/*!
 * @brief: Allocates the needed memory for an ab tree and bootstraps it
 */
ab_tree_t* create_ab_tree(u32 a, u32 b)
{
    ab_tree_t* ret;

    /* Check if the tree constraint hold up */
    if (a < 2 || b < (2 * a - 1))
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->a = a;
    ret->b = b;
    ret->root = __create_abt_node(ret);

    return ret;
}

/*!
 * @brief: Destroy the memory occupied by the ab tree
 *
 * Destroys all nodes recursively and finally kills the tree objects memory
 */
void destroy_ab_tree(ab_tree_t* tree)
{
    /* TODO: */
    kernel_panic("TODO: destroy_ab_tree");
}

/*!
 * @brief: Tries to find a key inside an abt node
 *
 * @pEntry: Puts the pointer to the entry here if we find our key
 * @pNExtNode: Puts a pointer to the next node we need to search in order to find our key
 */
static inline void __abt_node_find_key(abt_node_t* node, u64 key, abt_entry_t** pEntry, abt_node_t** pNextNode)
{
    u32 i;

    if (!node || !pEntry || !pNextNode)
        return;

    *pEntry = NULL;
    *pNextNode = NULL;

    for (i = 0; i < node->nr_entries; i++) {
        /* Find the first entry that is bigger than our key */
        if (node->vec_entries[i]->key > key)
            break;

        /* If we find our entry here, just return it */
        if (node->vec_entries[i]->key == key) {
            /* Export the entry */
            *pEntry = node->vec_entries[i];
            return;
        }
    }

    /* Export the next node to search */
    *pNextNode = node->vec_children[i];
}

int abt_find(ab_tree_t* tree, u64 key, void** pData)
{
    abt_node_t* cur_node;
    abt_node_t* next_node;
    abt_entry_t* cur_entry;

    cur_node = tree->root;

    do {
        /* Try to find the key here */
        __abt_node_find_key(cur_node, key, &cur_entry, &next_node);

        cur_node = next_node;
    } while (cur_node);

    if (!cur_entry)
        return -KERR_NOT_FOUND;

    *pData = cur_entry->data;

    return 0;
}

int abt_insert(ab_tree_t* tree, u64 key, void* data)
{
    abt_node_t* cur_node;

    cur_node = tree->root;

    do {
    } while (cur_node);

    return 0;
}
int abt_delete(ab_tree_t* tree, u64 key);

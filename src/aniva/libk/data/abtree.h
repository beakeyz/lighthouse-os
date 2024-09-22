#ifndef __ANIVA_LIBK_AB_TREE_H__
#define __ANIVA_LIBK_AB_TREE_H__

/* A single node inside the tree */
#include "libk/stddef.h"

struct abt_node;

/*
 * Root object for a single AB-tree
 *
 * This data structure is highly effective for IDed objects, like storing ctlcode implementations
 * for devices. It has a low memory complexity, relative to the old hashmap-like implementation and
 * it's also relatively fast (Time complexity is yet to be found xDD idk what it is exactly lol).
 */
typedef struct {
    /*
     * A/B parameters of this tree
     * all internal nodes except for the root have between a and b children,
     * where a and b are integers such that 2 ≤ a ≤ (b+1)/2. The root has,
     * if it is not a leaf, between 2 and b children
     */
    u32 a, b;
    struct abt_node* root;
} ab_tree_t;

ab_tree_t* create_ab_tree(u32 a, u32 b);
void destroy_ab_tree(ab_tree_t* tree);

int abt_find(ab_tree_t* tree, u64 key, void** pData);
int abt_insert(ab_tree_t* tree, u64 key, void* data);
int abt_delete(ab_tree_t* tree, u64 key);

#endif // !__ANIVA_LIBK_AB_TREE_H__

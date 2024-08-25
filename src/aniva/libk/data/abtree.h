#ifndef __ANIVA_LIBK_AB_TREE_H__
#define __ANIVA_LIBK_AB_TREE_H__

/* A single node inside the tree */
#include "libk/stddef.h"

struct abt_node;
struct abt_cluster;

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
    struct abt_cluster* root;
} ab_tree_t;

#endif // !__ANIVA_LIBK_AB_TREE_H__

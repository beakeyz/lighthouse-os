#include "abtree.h"

/*
 * A single cluster may contain multiple abtree nodes.
 * A cluster also knows where it's most left-bound next
 * entry is
 */
typedef struct abt_cluster {
    struct abt_cluster* next_cluster;
    struct abt_node* vec_nodes;
} abt_cluster_t;

typedef struct abt_node {
    u64 key;
    void* data;
    struct abt_cluster** p_leaf;
} abt_node_t;

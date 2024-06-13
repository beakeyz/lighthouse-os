#include "bin_tree.h"

binary_tree_t* create_binary_tree(f_bin_tree_comperator_t comperator);
void destroy_binary_tree(binary_tree_t* tree);

kerror_t binary_tree_insert(binary_tree_t* tree, void* data);
kerror_t binary_tree_remove(binary_tree_t* tree, void* data);

kerror_t binary_tree_find(binary_tree_t* tree, void* data);

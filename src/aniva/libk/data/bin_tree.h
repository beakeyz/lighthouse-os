#ifndef __ANIVA_TREE__
#define __ANIVA_TREE__

#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"

struct binary_tree;
struct binary_tree_node;

typedef bool (*f_bin_tree_comperator_t) (void* left, void* right);

typedef struct binary_tree {
  size_t m_depth;
  size_t m_node_count;

  f_bin_tree_comperator_t m_comperator;

  struct binary_tree_node* m_root;
} binary_tree_t;

typedef struct binary_tree_node {
  void* m_data;
  struct binary_tree_node* m_left;
  struct binary_tree_node* m_right;
} binary_tree_node_t;

binary_tree_t* create_binary_tree(f_bin_tree_comperator_t comperator);
void destroy_binary_tree(binary_tree_t* tree);

ErrorOrPtr binary_tree_insert(binary_tree_t* tree, void* data);
ErrorOrPtr binary_tree_remove(binary_tree_t* tree, void* data);

ErrorOrPtr binary_tree_find(binary_tree_t* tree, void* data);

#endif // !__ANIVA_TREE__

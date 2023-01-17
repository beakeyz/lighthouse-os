#ifndef __ANIVA_TREE__
#define __ANIVA_TREE__
#include "libk/linkedlist.h"
#include "stddef.h"

struct tree;
struct tree_node;

typedef struct tree {
  size_t m_depth;
  size_t m_node_count;
  struct tree_node* m_root_node_ptr;
} tree_t;

typedef struct tree_node {
  void* m_data;
  list_t m_entries;
  struct tree_node* m_parent_ptr;
} tree_node_t;

tree_t* init_tree();
tree_t* init_tree_with_root(void* data);

void tree_remove_node(tree_t* this, tree_node_t* node, bool preserve_entries);
void* tree_append_to_node(tree_t* this, tree_node_t* node, void* data);
// TODO:

#endif // !__ANIVA_TREE__

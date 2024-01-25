#include "node.h"
#include "libk/data/hashmap.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <mem/zalloc.h>

#define SOFT_OSS_NODE_OBJ_MAX 0x1000

static zone_allocator_t* _node_allocator;
static zone_allocator_t* _entry_allocator;

void init_oss_nodes()
{
  /* Initialize the caches */
  _node_allocator = create_zone_allocator(16 * Kib, sizeof(oss_node_t), NULL);
  _entry_allocator = create_zone_allocator(16 * Kib, sizeof(oss_node_entry_t), NULL);
}

/*!
 * @brief: Allocate memory for a oss node
 */
oss_node_t* create_oss_node(const char* name, enum OSS_NODE_TYPE type, struct oss_node_ops* ops, struct oss_node* parent)
{
  oss_node_t* ret;

  if (!name)
    return nullptr;

  ret = zalloc_fixed(_node_allocator);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->name = strdup(name);
  ret->type = type;
  ret->parent = parent;
  ret->ops = ops;

  ret->lock = create_mutex(NULL);

  /* Only object storing nodes need a hashmap */
  if (type == OSS_OBJ_STORE_NODE)
    /* TODO: Allow the hashmap to be resized */
    ret->obj_map = create_hashmap(SOFT_OSS_NODE_OBJ_MAX, NULL);

  return ret;
}

/*!
 * @brief: Clean up oss node memory
 */
void destroy_oss_node(oss_node_t* node)
{
  if (!node)
    return;

  kfree((void*)node->name);

  destroy_mutex(node->lock);

  if (node->obj_map)
    destroy_hashmap(node->obj_map);

  zfree_fixed(_node_allocator, node);
}

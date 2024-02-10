#include "node.h"
#include "libk/flow/error.h"
#include "obj.h"
#include "libk/data/hashmap.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <mem/zalloc.h>

/*
 * Core code for the oss nodes
 *
 * This code should really only supply basic routines to manipulate the node layout
 * The core handles any weird stuff with object generators
 */

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
 *
 * NOTE: It's OK for @ops and @parent to be NULL
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
  /* TODO: Allow the hashmap to be resized */
  ret->obj_map = create_hashmap(SOFT_OSS_NODE_OBJ_MAX, NULL);

  printf(" => Creating node: %s\n", ret->name);
  return ret;
}

/*!
 * @brief: Clean up oss node memory
 */
void destroy_oss_node(oss_node_t* node)
{
  oss_node_entry_t* entry;

  if (!node)
    return;

  entry = nullptr;

  /* Remove ourselves if we're still attached */
  if (node->parent)
    oss_node_remove_entry(node->parent, node->name, &entry);

  if (entry)
    destroy_oss_node_entry(entry);

  /* Make sure there are no lingering objects */
  oss_node_clean_objects(node);

  printf(" <= Destroying node: %s\n", node->name);

  kfree((void*)node->name);

  destroy_mutex(node->lock);
  destroy_hashmap(node->obj_map);

  zfree_fixed(_node_allocator, node);
}

/*!
 * @brief: Find the private entry for a given node
 *
 * When @node does not have it's own private field, we walk the parent list
 * until we find a useable field
 */
void* oss_node_unwrap(oss_node_t* node)
{
  if (node->priv)
    return node->priv;

  if (!node->parent)
    return nullptr;

  do {
    node = node->parent;
  } while (node && !node->priv);

  if (!node)
    return nullptr;

  return node->priv;
}

/*!
 * @brief: Check if there are any invalid chars in the name
 */
static bool _valid_entry_name(const char* name)
{
  while (*name) {

    /* OSS node entry may not have a path seperator in it's name */
    if (*name == '/')
      return false;

    name++;
  }

  return true;
}

/*!
 * @brief: Check if a certain oss entry already is already added in @node
 *
 * The nodes lock should be taken by the caller. Is also assumed that the
 * caller has verified the integrity of @entry
 */
static inline bool _node_contains_entry(oss_node_t* node, oss_node_entry_t* entry)
{
  const char* entry_name;

  entry_name = oss_node_entry_getname(entry);

  /* How the fuck does this entry not have a name? */
  if (!entry_name)
    return false;

  return (hashmap_get(node->obj_map, (hashmap_key_t)entry_name) != nullptr);
}

/*!
 * @brief: Add a oss_obj to this node
 *
 * This function does not add entries recursively
 */
int oss_node_add_obj(oss_node_t* node, struct oss_obj* obj)
{
  int error;
  oss_node_entry_t* entry;

  if (!node || !obj)
    return -1;

  if (!_valid_entry_name(obj->name))
    return -2;

  entry = create_oss_node_entry(OSS_ENTRY_OBJECT, obj);

  if (!entry)
    return -3;

  mutex_lock(node->lock);

  error = -1;

  if (_node_contains_entry(node, entry))
    goto unlock_and_exit;

  error = IsError(hashmap_put(node->obj_map, (hashmap_key_t)obj->name, entry));

  if (!error)
    obj->parent = node;
  
unlock_and_exit:
  mutex_unlock(node->lock);
  return error;
}

/*!
 * @brief: Add a nested oss node to @node
 *
 * This function does not add entries recursively
 */
int oss_node_add_node(oss_node_t* node, struct oss_node* obj)
{
  int error;
  oss_node_entry_t* entry;

  if (!node || !obj)
    return -1;

  if (!_valid_entry_name(obj->name))
    return -2;

  entry = create_oss_node_entry(OSS_ENTRY_NESTED_NODE, obj);

  if (!entry)
    return -3;

  mutex_lock(node->lock);

  error = -1;

  if (_node_contains_entry(node, entry))
    goto unlock_and_exit;

  error = IsError(hashmap_put(node->obj_map, (hashmap_key_t)obj->name, entry));

  if (!error)
    obj->parent = node;
  
unlock_and_exit:
  mutex_unlock(node->lock);
  return error;
}

static inline void _entry_on_detach(oss_node_entry_t* entry)
{
  switch (entry->type) {
    case OSS_ENTRY_OBJECT:
      entry->obj->parent = nullptr;
      break;
    case OSS_ENTRY_NESTED_NODE:
      entry->node->parent = nullptr;
      break;
  }
}

/*!
 * @brief: Remove an entry from @node
 *
 * NOTE: when the entry is a nested node, that means that all entries nested in this removed
 * node are also removed from @node
 */
int oss_node_remove_entry(oss_node_t* node, const char* name, struct oss_node_entry** entry_out)
{
  int error;
  oss_node_entry_t* entry;

  if (!node || !name)
    return -1;

  /* If the name is invalid we don't even have to go through all this fuckery */
  if (!_valid_entry_name(name))
    return -2;

  /* Lock because we don't want do die */
  mutex_lock(node->lock);

  /* Remove and get the thing */
  error = -3;
  entry = hashmap_remove(node->obj_map, (hashmap_key_t)name);

  if (!entry)
    goto unlock_and_exit;
  
  /* Only return if the caller means to */
  if (entry_out)
    *entry_out = entry;

  /* Make sure we tell the entry it's detached */
  _entry_on_detach(entry);
  error = 0;
unlock_and_exit:
  mutex_unlock(node->lock);
  return error;
}

/*!
 * @brief: Find a singular entry inside a node
 *
 * This only searches the nodes object map, regardless of this node being an
 * object generator or not
 */
int oss_node_find(oss_node_t* node, const char* name, struct oss_node_entry** entry_out)
{
  oss_node_entry_t* entry;

  if (!entry_out)
    return -1;

  /* Set to NULL to please fuckers that only check this variable -_- */
  *entry_out = NULL;

  /* If the name is invalid we don't even have to go through all this fuckery */
  if (!_valid_entry_name(name))
    return -2;

  /* Lock because we don't want do die */
  mutex_lock(node->lock);

  /* Get the thing */
  entry = hashmap_get(node->obj_map, (hashmap_key_t)name);

  if (!entry)
    goto unlock_and_exit;
  
  *entry_out = entry;

  mutex_unlock(node->lock);
  return 0;

unlock_and_exit:
  mutex_unlock(node->lock);
  return -3;
}

int oss_node_query(oss_node_t* node, const char* path, struct oss_obj** obj_out)
{
  if (!obj_out || !node || !node->ops)
    return -1;

  /* Can't query on a non-gen node =/ */
  if (node->type != OSS_OBJ_GEN_NODE)
    return -2;

  mutex_lock(node->lock);

  *obj_out = node->ops->f_open(node, path);

  mutex_unlock(node->lock);
  return (*obj_out ? 0 : -1);
}

static ErrorOrPtr _node_itter(void* v, uint64_t arg)
{
  int error;
  oss_node_entry_t* entry = v;
  oss_node_t* node;
  oss_obj_t* obj;
  bool (*f_itter)(oss_node_t* node, struct oss_obj* obj) = (void*)arg;

  if (!entry)
    return Success(0);

  error = 0;

  switch (entry->type) {
    /* In case of an object, simply call it in the itteration */
    case OSS_ENTRY_OBJECT:
      obj = entry->obj;
      node = obj->parent;

      if (!f_itter(node, obj))
        return Error();
      break;
    /* Itterate over nodes recursively */
    case OSS_ENTRY_NESTED_NODE:
      node = entry->node;

      error = oss_node_itterate(node, f_itter);
      break;
    default:
      break;
  }

  return error ? Error() : Success(0);
}

/*!
 * @brief: Walk all the child objects under @node
 */
int oss_node_itterate(oss_node_t* node, bool(*f_itter)(oss_node_t* node, struct oss_obj* obj))
{
  printf("Itterating: %s \n", node->name);
  return IsError(hashmap_itterate(node->obj_map, _node_itter, (uint64_t)f_itter));
}

/*!
 * @brief: Walks the object map of @node recursively and destroys every object if finds
 */
int oss_node_clean_objects(oss_node_t* node)
{
  int error;
  uintptr_t* array;
  size_t size;
  size_t entry_count;

  mutex_lock(node->lock);

  error = hashmap_to_array(node->obj_map, &array, &size);

  if (error)
    goto unlock_and_exit;

  entry_count = size / (sizeof(*array));

  for (uintptr_t i = 0; i < entry_count; i++) {
    oss_node_entry_t* entry = (oss_node_entry_t*)((uintptr_t)array[i]);

    switch (entry->type) {
      case OSS_ENTRY_NESTED_NODE:
        destroy_oss_node(entry->node);
        break;
      case OSS_ENTRY_OBJECT:
        destroy_oss_obj(entry->obj);
        break;
    }

    destroy_oss_node_entry(entry);
  }

  kfree(array);

unlock_and_exit:
  mutex_unlock(node->lock);
  return error;
}

oss_node_entry_t* create_oss_node_entry(enum OSS_ENTRY_TYPE type, void* obj)
{
  oss_node_entry_t* ret;

  ret = zalloc_fixed(_entry_allocator);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->type = type;
  ret->ptr = obj;

  return ret;
}

void destroy_oss_node_entry(oss_node_entry_t* entry)
{
  zfree_fixed(_entry_allocator, entry);
}


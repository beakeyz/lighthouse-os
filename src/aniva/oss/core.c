#include "core.h"
#include "libk/data/hashmap.h"
#include <libk/string.h>
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"

static hashmap_t* _rootnode_map = nullptr;
static mutex_t* _core_lock = nullptr;
static oss_node_t* _local_root = nullptr;
static const char* _local_root_name = ":";

void oss_test() {

  oss_obj_t* out;

  printf("Creating path\n");
  /* Create a path */
  oss_create_path(nullptr, "Root/Test/Fuck");

  printf("Adding obj\n");
  /* Attach an object to that path */
  oss_attach_obj(":/Root/Test/Fuck", create_oss_obj("test"));

  printf("Resolving obj\n");
  oss_resolve_obj(":/Root/Test/Fuck/test", &out);

  printf("Got oss_obj: %s\n", out ? out->name : "NULL");

  destroy_oss_obj(out);

  oss_resolve_obj(":/Root/Test/Fuck/test", &out);

  printf("Got oss_obj: %s\n", out ? out->name : "NULL");
}

/*!
 * @brief: Find the path entry in @fullpath that is pointed to by @idx
 */
static const char* _get_pathentry_at_idx(const char* fullpath, uint32_t idx)
{
  const char* path_idx = fullpath;
  const char* c_subentry_start = fullpath;

  while (*path_idx) {

    if (*path_idx != '/')
      goto cycle;

    if (!idx) {

      /* This is fucked lmao */
      if (!(*c_subentry_start))
        break;

      return c_subentry_start;
    }

    idx--;
  
    /* Set the current subentry to the next segment */
    c_subentry_start = path_idx+1;
cycle:
    path_idx++;
  }

  /* If we haven't reached the end yet, something went wrong */
  if (idx)
    return nullptr;

  return c_subentry_start;
}

static const char* _find_path_subentry_at(const char* path, uint32_t idx)
{
  const char* ret = nullptr;
  char* path_cpy = strdup(path);
  char* tmp;

  /* Non-const in, non-const out =) */
  tmp = (char*)_get_pathentry_at_idx(path_cpy, idx);

  /* Fuck man */
  if (!tmp)
    goto free_and_exit;

  /* Trim off the next pathentry spacer */
  for (uint32_t i = 0; tmp[i]; i++) {
    if (tmp[i] == '/') {
      tmp[i] = NULL;
      break;
    }
  }

  ret = strdup(tmp);

free_and_exit:
  kfree((void*)path_cpy);
  return ret;
}

static oss_node_t* _get_rootnode(const char* name)
{
  if (!name)
    return _local_root;

  return hashmap_get(_rootnode_map, (hashmap_key_t)name);
}

/*!
 * @brief: Resolve a oss_object relative from an oss_node
 *
 * If @rel is null, we default to _local_root
 */
int oss_resolve_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out)
{
  int error;
  const char* this_name;
  oss_node_t* obj_gen;
  oss_node_t* c_node;
  oss_node_entry_t* c_entry;
  uint32_t c_idx;
  uint32_t obj_gen_path_idx;

  if (!rel)
    rel = _local_root;

  c_node = rel;
  c_idx = 0;
  obj_gen = nullptr;

  /* If the mutex is already taken by another routine (like oss_resolve_obj) don't do shit */
  if (!mutex_is_locked(_core_lock))
    mutex_lock(_core_lock);

  while ((this_name = _find_path_subentry_at(path, c_idx++))) {

    /* Save the object generation node */
    if (c_node->type == OSS_OBJ_GEN_NODE && !obj_gen) {
      obj_gen = c_node;
      obj_gen_path_idx = c_idx-1;
    }

    error = oss_node_find(c_node, this_name, &c_entry);

    kfree((void*)this_name);

    if (error || !c_entry)
      break;

    if (c_entry->type != OSS_ENTRY_NESTED_NODE)
      break;

    c_node = c_entry->node;
  }

  /* If we've failed to find a cached object, query the object generation node */
  if (obj_gen && !c_entry) {
    const char* rel_path = _get_pathentry_at_idx(path, obj_gen_path_idx);

    error = oss_node_query(obj_gen, rel_path, out);
  }

  if (c_entry)
    *out = c_entry->obj;

  mutex_unlock(_core_lock);
  return error;
}

/*!
 * @brief: Looks through the registered nodes and tries to resolve a oss_obj out of @path
 *
 * These paths are always absolute
 */
int oss_resolve_obj(const char* path, oss_obj_t** out)
{
  const char* this_name;
  const char* rel_path;
  oss_node_t* c_node;

  if (!out || !path)
    return -1;

  *out = NULL;

  mutex_lock(_core_lock);

  /* Find the root node name */
  this_name = _find_path_subentry_at(path, 0);

  /* Try to fetch the rootnode */
  c_node = _get_rootnode(this_name);

  /* Clean up */
  if (this_name)
    kfree((void*)this_name);

  /* Invalid path =/ */
  if (!c_node)
    return -1;

  rel_path = path;

  while (*rel_path) {
    if (*rel_path++ == '/')
      break;
  }

  ASSERT_MSG(*rel_path != '/', "Ur logic broke down in oss_resolve_oss_obj");

  return oss_resolve_obj_rel(c_node, rel_path, out);
}

/*!
 * @brief: Looks through the registered nodes and tries to resolve a node out of @path
 */
int oss_resolve_node(const char* path, oss_node_t** out)
{
  const char* this_name;
  oss_node_t* c_node;
  oss_node_entry_t* c_entry;
  uint32_t c_idx;

  if (!out || !path)
    return -1;

  *out = NULL;

  mutex_lock(_core_lock);

  /* Find the root node name */
  this_name = _find_path_subentry_at(path, 0);

  /* Try to fetch the rootnode */
  c_node = _get_rootnode(this_name);

  /* Clean up */
  if (this_name)
    kfree((void*)this_name);

  /* Invalid path =/ */
  if (!c_node)
    goto unlock_and_error;

  c_idx = 1;

  while ((this_name = _find_path_subentry_at(path, c_idx++))) {
    oss_node_find(c_node, this_name, &c_entry);

    kfree((void*)this_name);

    if (!c_entry || c_entry->type != OSS_ENTRY_NESTED_NODE)
      goto unlock_and_error;

    c_node = c_entry->node;
  }

  *out = c_node;

  mutex_unlock(_core_lock);
  return 0;

unlock_and_error:
  mutex_unlock(_core_lock);
  return -1;
}

/*!
 * @brief: Creates a chain of nodes for @path
 */
int oss_create_path(struct oss_node* node, const char* path)
{
  int error;
  uint32_t idx;
  oss_node_t* c_node;
  oss_node_entry_t* c_entry;
  const char* this_name;

  if (!node)
    node = _local_root;

  idx = 0;
  c_node = node;

  while ((this_name = _find_path_subentry_at(path, idx++))) {

    /* Find this entry */
    error = oss_node_find(c_node, this_name, &c_entry);

    /* We can't create a path if it points to an object */
    if (!error && c_entry->type == OSS_ENTRY_OBJECT)
        goto free_and_exit;

    /* Could not find this bitch, create a new node */
    if (error || !c_entry) {
      oss_node_add_node(c_node, create_oss_node(this_name, OSS_OBJ_STORE_NODE, NULL, NULL));

      error = oss_node_find(c_node, this_name, &c_entry);

      /* Failed twice, this is bad lmao */
      if (error)
        goto free_and_exit;
    }

    ASSERT_MSG(c_entry->type == OSS_ENTRY_NESTED_NODE, "Somehow created a non-nested node entry xD");

    kfree((void*)this_name);
    c_node = c_entry->node;

    continue;

free_and_exit:
    kfree((void*)this_name);
    return -1;
  }

  return 0;
}

/*!
 * @brief: Attach @node to @path
 *
 * There must be a node attached to the end of @path
 */
int oss_attach_node(const char* path, struct oss_node* node)
{
  int error;
  oss_node_t* c_node;

  error = oss_resolve_node(path, &c_node);

  if (error)
    return error;

  mutex_lock(_core_lock);

  error = oss_node_add_node(c_node, node);
  
  mutex_unlock(_core_lock);
  return error;
}

int oss_attach_obj(const char* path, struct oss_obj* obj)
{
  int error;
  oss_node_t* node;

  error = oss_resolve_node(path, &node);

  if (error)
    return error;

  mutex_lock(_core_lock);

  error = oss_node_add_obj(node, obj);
  
  mutex_unlock(_core_lock);
  return error;
}

/*!
 * @brief: Create OSS root node registry
 */
void init_oss()
{
  _core_lock = create_mutex(NULL);
  _rootnode_map = create_hashmap(128, NULL);

  init_oss_nodes();

  /* Create the local root */
  _local_root = create_oss_node(_local_root_name, OSS_OBJ_STORE_NODE, NULL, NULL);

  /* First rootnode in our map */
  hashmap_put(_rootnode_map, (hashmap_key_t)_local_root_name, _local_root);
}


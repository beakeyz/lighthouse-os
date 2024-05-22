#include "core.h"
#include "fs/core.h"
#include <libk/string.h>
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/malloc.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"

static oss_node_t* _rootnode = nullptr;
static mutex_t* _core_lock = nullptr;

static struct oss_node* _oss_create_path_abs_locked(const char* path);
static struct oss_node* _oss_create_path_locked(struct oss_node* node, const char* path);
static int _oss_resolve_node_locked(const char* path, oss_node_t** out);
static int _oss_resolve_node_rel_locked(struct oss_node* rel, const char* path, struct oss_node** out);
static int _oss_resolve_obj_rel_locked(struct oss_node* rel, const char* path, struct oss_obj** out);

static inline int _oss_attach_rootnode_locked(struct oss_node* node);
static inline int _oss_detach_rootnode_locked(struct oss_node* node);

/*!
 * @brief: Quick routine to log the current state of the kernel heap
 *
 * TEMP: (TODO) remove this shit
 */
static inline void _oss_test_mem()
{
  memory_allocator_t alloc;

  kheap_copy_main_allocator(&alloc);
  printf(" (*) Got %lld/%lld bytes free\n", alloc.m_free_size, alloc.m_used_size+alloc.m_free_size);
}

/*!
 * @brief: Quick routine to test the capabilities and integrity of OSS
 *
 * Confirmations:
 *  - oss_resolve can creatre memory leak
 *  - leak is not in any string duplications, since changing the path does not change the 
 *    amount of bytes leaked
 */
void oss_test()
{
  oss_obj_t* init_obj;

  _oss_test_mem();

  oss_resolve_obj_rel(nullptr, "Root/System/test.drv", &init_obj);
  destroy_oss_obj(init_obj);

  _oss_test_mem();

  oss_resolve_obj_rel(nullptr, "Root/System/test.drv", &init_obj);
  destroy_oss_obj(init_obj);

  _oss_test_mem();

  kernel_panic("End of oss_test");
}

/*!
 * @brief: Find the path entry in @fullpath that is pointed to by @idx
 */
static const char* _get_pathentry_at_idx(const char* fullpath, uint32_t idx)
{
  const char* path_idx = fullpath;
  const char* c_subentry_start = fullpath;

  // System/ 
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

  if (!(*c_subentry_start))
    return nullptr;

  return c_subentry_start;
}

static const char* _find_path_subentry_at(const char* path, uint32_t idx)
{
  uint32_t i;
  const char* ret = nullptr;
  char* path_cpy = strdup(path);
  char* tmp;

  /* Non-const in, non-const out =) */
  tmp = (char*)_get_pathentry_at_idx(path_cpy, idx);

  /* Fuck man */
  if (!tmp)
    goto free_and_exit;

  /* Trim off the next pathentry spacer */
  for (i = 0; tmp[i]; i++) {
    if (tmp[i] == '/') {
      tmp[i] = NULL;
      break;
    }
  }

  if (!i)
    goto free_and_exit;

  ret = strdup(tmp);

free_and_exit:
  kfree((void*)path_cpy);
  return ret;
}

static oss_node_t* _get_rootnode(const char* name)
{
  oss_node_entry_t* entry;

  if (!name)
    return nullptr;

  entry = nullptr;

  if (oss_node_find(_rootnode, name, &entry))
    return nullptr;

  if (!entry || entry->type != OSS_ENTRY_NESTED_NODE)
    return nullptr;

  return entry->node;
}

static inline oss_node_t* _get_rootnode_from_path(const char* path, uint32_t* idx)
{
  oss_node_t* ret;
  const char* this_name = _find_path_subentry_at(path, 0);

  if (!this_name)
    return nullptr;

  ret = _get_rootnode(this_name);

  kfree((void*)this_name);

  if (ret)
    (*idx)++;

  return ret;
}

static int _oss_resolve_obj_rel_locked(struct oss_node* rel, const char* path, struct oss_obj** out)
{
  int error;
  const char* this_name;
  oss_node_t* obj_gen;
  oss_node_t* c_node;
  oss_node_entry_t* c_entry;
  uint32_t c_idx;
  uint32_t obj_gen_path_idx;

  ASSERT_MSG(_core_lock->m_lock_holder, "Tried to call _oss_resolve_obj_rel_locked without having locked oss");

  c_node = rel;
  c_idx = 0;
  obj_gen = nullptr;

  /* No relative node. Fetch the rootnode */
  if (!c_node) {
    c_node = _get_rootnode_from_path(path, &c_idx);

    if (!c_node) return -1;
  }

  while ((this_name = _find_path_subentry_at(path, c_idx++))) {

    if (strncmp(this_name, "..", 2) == 0) {
      if (c_node)
        c_node = c_node->parent;

      kfree((void*)this_name);
      continue;
    }

    if (strncmp(this_name, ".", 1) == 0) {
      kfree((void*)this_name);
      continue;
    }

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

    /* Query the generation node for an object */
    return oss_node_query(obj_gen, rel_path, out);
  }

  /* Did we find an entry? */
  if (!c_entry || c_entry->type != OSS_ENTRY_OBJECT)
    return error;

  /* Add a reference to the object */
  oss_obj_ref(c_entry->obj);

  /* Export the reference */
  *out = c_entry->obj;
  return error;
}

/*!
 * @brief: Resolve a oss_object relative from an oss_node
 *
 * If @rel is null, we default to _local_root
 */
int oss_resolve_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out)
{
  int error;

  mutex_lock(_core_lock);

  error = _oss_resolve_obj_rel_locked(rel, path, out);

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
  int error;
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

  error = -1;

  /* Invalid path =/ */
  if (!c_node)
    goto unlock_and_error;

  rel_path = path;

  while (*rel_path) {
    if (*rel_path++ == '/')
      break;
  }

  error = _oss_resolve_obj_rel_locked(c_node, rel_path, out);

unlock_and_error:
  mutex_unlock(_core_lock);
  return error;
}

static int _oss_resolve_node_rel_locked(struct oss_node* rel, const char* path, struct oss_node** out)
{
  oss_node_t* c_node;
  oss_node_t* gen_node;
  oss_node_entry_t* c_entry;
  const char* this_name;
  const char* rel_path;
  uint32_t c_idx;
  uint32_t gen_path_idx;

  c_node = rel;
  gen_node = nullptr;
  gen_path_idx = 0;
  c_idx = 0;

  if (!c_node) {
    c_node = _get_rootnode_from_path(path, &c_idx);

    if (!c_node) return -1;
  }

  while (*path && (this_name = _find_path_subentry_at(path, c_idx++))) {

    /* Also check this mofo */
    if (strncmp(this_name, "..", 2) == 0) {
      if (c_node)
        c_node = c_node->parent;

      kfree((void*)this_name);
      continue;
    }

    /* Check this mofo */
    if (strncmp(this_name, ".", 1) == 0) {
      kfree((void*)this_name);
      continue;
    }

    if (c_node->type == OSS_OBJ_GEN_NODE && !gen_node) {
      gen_node = c_node;
      gen_path_idx = c_idx-1;
    }

    /* Try to find a node */
    oss_node_find(c_node, this_name, &c_entry);

    /* Free the name */
    kfree((void*)this_name);

    /* Clear the current node just in case we break here */
    c_node = nullptr;

    if (!oss_node_entry_has_node(c_entry))
      break;

    /* Set the node again */
    c_node = c_entry->node;
  }

  if (!c_node && gen_node) {
    rel_path = _get_pathentry_at_idx(path, gen_path_idx);

    if (!rel_path)
      return -1;

    return oss_node_query_node(gen_node, rel_path, out);
  }

  if (!c_node)
    return -1;

  *out = c_node;
  return 0;
}


int oss_resolve_node_rel(struct oss_node* rel, const char* path, struct oss_node** out)
{
  int error;

  mutex_lock(_core_lock);

  error = _oss_resolve_node_rel_locked(rel, path, out);

  mutex_unlock(_core_lock);

  return error;
}

static int _oss_resolve_node_locked(const char* path, oss_node_t** out)
{
  if (!out || !path)
    return -1;

  *out = NULL;

  return _oss_resolve_node_rel_locked(_rootnode, path, out);
}


/*!
 * @brief: Looks through the registered nodes and tries to resolve a node out of @path
 */
int oss_resolve_node(const char* path, oss_node_t** out)
{
  int error;

  mutex_lock(_core_lock);

  error = _oss_resolve_node_locked(path, out);

  mutex_unlock(_core_lock);

  return error;
}

static struct oss_node* _oss_create_path_locked(struct oss_node* node, const char* path)
{
  int error;
  uint32_t c_idx;
  oss_node_t* c_node;
  oss_node_t* new_node;
  oss_node_entry_t* c_entry;
  const char* this_name;

  c_idx = 0;
  c_node = node;
  new_node = nullptr;
  
  if (!c_node) {
    c_node = _get_rootnode_from_path(path, &c_idx);

    if (!c_node) return nullptr;
  }

  while (*path && (this_name = _find_path_subentry_at(path, c_idx++))) {

    /* Find this entry */
    error = oss_node_find(c_node, this_name, &c_entry);

    /* We can't create a path if it points to an object */
    if (!error && c_entry->type == OSS_ENTRY_OBJECT)
        goto free_and_exit;

    /* Could not find this bitch, create a new node */
    if (error || !c_entry) {
      new_node = create_oss_node(this_name, OSS_OBJ_STORE_NODE, NULL, NULL);

      error = oss_node_add_node(c_node, new_node);

      if (error)
        goto free_and_exit;

      error = oss_node_find(c_node, this_name, &c_entry);

      /* Failed twice, this is bad lmao */
      if (error)
        goto free_and_exit;

      new_node = nullptr;
    }

    ASSERT_MSG(c_entry->type == OSS_ENTRY_NESTED_NODE, "Somehow created a non-nested node entry xD");

    kfree((void*)this_name);
    c_node = c_entry->node;

    continue;

free_and_exit:
    if (new_node)
      destroy_oss_node(new_node);

    kfree((void*)this_name);
    return nullptr;
  }

  /* If we gracefully exit the loop, the creation succeeded */
  return c_node;
}

/*!
 * @brief: Creates a chain of nodes for @path
 *
 * @returns: The last node it created
 */
struct oss_node* oss_create_path(struct oss_node* node, const char* path)
{
  oss_node_t* ret;

  mutex_lock(_core_lock);

  ret = _oss_create_path_locked(node, path);

  mutex_unlock(_core_lock);

  return ret;
}

/*!
 * @brief: Internal function to call when the core lock is locked
 */
static struct oss_node* _oss_create_path_abs_locked(const char* path)
{
  const char* this_name;
  const char* rel_path;
  oss_node_t* c_node;

  if (!path)
    return nullptr;

  /* Find the root node name */
  this_name = _find_path_subentry_at(path, 0);

  /* Try to fetch the rootnode */
  c_node = _get_rootnode(this_name);

  /* Clean up */
  if (this_name)
    kfree((void*)this_name);

  /* Invalid path =/ */
  if (!c_node)
    return nullptr;

  rel_path = path;

  while (*rel_path) {
    if (*rel_path++ == '/')
      break;
  }

  return _oss_create_path_locked(c_node, rel_path);
}

/*!
 * @brief: Creates an absolute path
 */
struct oss_node* oss_create_path_abs(const char* path)
{
  oss_node_t* ret;
  
  mutex_lock(_core_lock);

  ret = _oss_create_path_abs_locked(path);

  mutex_unlock(_core_lock);

  return ret;
}

static inline int _oss_attach_rootnode_locked(struct oss_node* node)
{
  return oss_node_add_node(_rootnode, node);
}

int oss_attach_rootnode(struct oss_node* node)
{
  int error;

  if (!node)
    return -1;

  mutex_lock(_core_lock);

  error = _oss_attach_rootnode_locked(node);

  mutex_unlock(_core_lock);
  return error;
}

static inline int _oss_detach_rootnode_locked(struct oss_node* node)
{
  oss_node_entry_t* entry;

  if (oss_node_remove_entry(_rootnode, node->name, &entry))
    return -1;

  destroy_oss_node_entry(entry);
  return 0;
}

int oss_detach_rootnode(struct oss_node* node)
{
  int error;

  if (!node)
    return -1;

  mutex_lock(_core_lock);

  error = _oss_detach_rootnode_locked(node);

  mutex_unlock(_core_lock);
  return error;
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

  if (!path)
    return -1;

  error = oss_resolve_node(path, &c_node);

  if (error)
    return error;

  mutex_lock(_core_lock);

  error = oss_node_add_node(c_node, node);
  
  mutex_unlock(_core_lock);
  return error;
}

static int _try_path_remove_obj_name(char* path, const char* name)
{
  size_t path_len;
  size_t name_len;
  char* embedded_name;

  if (!path || !name)
    return -1;

  path_len = strlen(path);
  name_len = strlen(name);

  if (!path_len || !name_len)
    return -1;

  /* Path can't contain name */
  if (strlen(path) < strlen(name))
    return -1;

  embedded_name = &path[path_len-name_len];

  /* Compare */
  if (strcmp(embedded_name, name) || path[path_len-name_len-1] != '/')
    return -1;

  /* Remove the slash so we cut off the name at the end of the path */
  path[path_len-name_len-1] = NULL;
  return 0;
}

/*!
 * @brief: Attach an object relative of another node
 */
int oss_attach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj* obj)
{
  int error;
  oss_node_t* target_node;
  char* path_cpy;

  error = -1;
  /* Copy the path */
  path_cpy = strdup(path);

  /* Try to remove the name of the object, if it is present in the path */
  (void)_try_path_remove_obj_name(path_cpy, obj->name);

  /* Perpare a path */
  target_node = oss_create_path(rel, path_cpy);

  if (!target_node)
    goto dealloc_and_exit;

  /* Attach the object to the last node in the reated chain */
  error = oss_node_add_obj(target_node, obj);

dealloc_and_exit:
  kfree((void*)path_cpy);
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
 * @brief: Attach a filesystem with node name @rootname to @path
 *
 */
int oss_attach_fs(const char* path, const char* rootname, const char* fs, partitioned_disk_dev_t* device)
{
  int error;
  oss_node_t* node;
  oss_node_t* mountpoint_node;
  fs_type_t* fstype;

  /* Try to get the filsystem type we want to mount */
  fstype = get_fs_type(fs);

  if (!fstype)
    return -1;

  /* Let the filesystem driver generate a node for us */
  node = fstype->f_mount(fstype, rootname, device);

  if (!node)
    return -1;

  /* Lock the core */
  mutex_lock(_core_lock);

  error = -1;
  /* Create a path to mount on */
  mountpoint_node = _oss_create_path_abs_locked(path);

  /* No mountpoint node means we mount as a rootnode */
  if (!mountpoint_node)
    error = _oss_attach_rootnode_locked(node);
  else
    /* Add the generated node into the oss tree */
    error = oss_node_add_node(mountpoint_node, node);

  if (error)
    goto unmount_and_error;

  /* Unlock and return */
  mutex_unlock(_core_lock);
  return error;

unmount_and_error:
  fstype->f_unmount(fstype, node);
  destroy_oss_node(node);

  mutex_unlock(_core_lock);
  return error;
}

/*!
 * @brief: Detach a filesystem node from @path
 *
 * This does not destroy the oss node nor it's attached children
 *
 * FIXME: is this foolproof?
 */
int oss_detach_fs(const char* path, struct oss_node** out)
{
  int error;
  fs_oss_node_t* fs;
  oss_node_t* node;
  oss_node_t* parent;

  if (out)
    *out = NULL;

  mutex_lock(_core_lock);

  /* Find the node we want to detach */
  error = _oss_resolve_node_locked(path, &node);

  if (error)
    goto exit_and_unlock;

  if (node->type != OSS_OBJ_GEN_NODE)
    goto exit_and_unlock;

  fs = oss_node_getfs(node); 

  /* Make sure the fs driver unmounts our bby */
  error = fs->m_type->f_unmount(fs->m_type, node);

  ASSERT_MSG(error == KERR_NONE, " ->f_unmount call failed");

  parent = node->parent;

  if (!parent)
    error = _oss_detach_rootnode_locked(node);
  else
    /* Remove the node from it's parent */
    error = oss_node_remove_entry(parent, node->name, NULL);

  if (error)
    goto exit_and_unlock;

  if (out)
    *out = node;

exit_and_unlock:
  mutex_unlock(_core_lock);
  return error;
}

/*!
 * @brief: Remove an object from it's path
 */
int oss_detach_obj(const char* path, struct oss_obj** out)
{
  kernel_panic("TODO: oss_detach_obj");
}

/*!
 * @brief: Remove an object from it's path, relative from @rel
 */
int oss_detach_obj_rel(struct oss_node* rel, const char* path, struct oss_obj** out)
{
  kernel_panic("TODO: oss_detach_obj_rel");
}

const char* oss_get_objname(const char* path)
{
  uint32_t c_idx;
  const char* this_name;
  const char* next_name;

  c_idx = 0;

  while ((this_name = _find_path_subentry_at(path, c_idx++))) {
    next_name = _find_path_subentry_at(path, c_idx);

    if (!next_name)
      break;

    kfree((void*)next_name);
    kfree((void*)this_name);
  }

  return this_name;
}

/*!
 * @brief: Create OSS root node registry
 */
void init_oss()
{
  init_oss_nodes();

  _core_lock = create_mutex(NULL);
  _rootnode = create_oss_node("%", OSS_OBJ_STORE_NODE, nullptr, nullptr);
}


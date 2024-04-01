#include "dir.h"
#include "dev/core.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "libk/string.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"

/*!
 * @brief: Create a directory at the end of @path
 *
 * Fails if there is already a directory attached to the target node
 *
 * @root: The root node of the filesystem this directory comes from
 * @path: The path relative from @root
 */
dir_t* create_dir(oss_node_t* root, const char* path, struct dir_ops* ops, void* priv, uint32_t flags)
{
  dir_t* dir;
  const char* name;

  /* NOTE: This allocates @name on the heap */
  name = oss_get_objname(path);

  if (!name)
    return nullptr;

  dir = kmalloc(sizeof(*dir));

  if (!dir)
    return nullptr;

  memset(dir, 0, sizeof(*dir));

  dir->node = oss_create_path(root, path);
  dir->name = name;
  dir->ops = ops;
  dir->priv = priv;
  dir->flags = flags;
  dir->lock = create_mutex(NULL);

  init_atomic_ptr(&dir->ref, 0);
  
  /* Try to attach */
  if (oss_node_attach_dir(dir->node, dir))
    goto dealloc_and_fail;

  return dir;

dealloc_and_fail:
  kfree(dir);
  return nullptr;
}

static void dir_ref(dir_t* dir)
{
  if (!dir)
    return;

  mutex_lock(dir->lock);

  /* Increase the refcount by one */
  atomic_ptr_write(&dir->ref,
    atomic_ptr_read(&dir->ref)+1
  );

  mutex_unlock(dir->lock);
}

static void dir_unref(dir_t* dir)
{
  uint32_t refc;

  if (!dir)
    return;

  mutex_lock(dir->lock);

  refc = atomic_ptr_read(&dir->ref);

  if (!refc)
    goto unlock_and_exit;

  /* Increase the refcount by one */
  atomic_ptr_write(&dir->ref,
    (refc = refc-1)
  );

unlock_and_exit:
  mutex_unlock(dir->lock);

  if (!refc)
    destroy_dir(dir);
}

void destroy_dir(dir_t* dir)
{
  if (!dir)
    return;

  /* Detach first */
  if (dir->node)
    oss_node_detach_dir(dir->node);

  if (dir->ops && dir->ops->f_destroy)
    dir->ops->f_destroy(dir);

  destroy_mutex(dir->lock);

  kfree((void*)dir->name);
  kfree(dir);
}

int dir_create_child(dir_t* dir, const char* name)
{
  if (!dir || !name)
    return -KERR_INVAL;

  if (!dir->ops || !dir->ops->f_create_child)
    return -KERR_NULL;

  return dir->ops->f_create_child(dir, name);
}

int dir_remove_child(dir_t* dir, const char* name)
{
  if (!dir || !name)
    return -KERR_INVAL;

  if (!dir->ops || !dir->ops->f_remove_child)
    return -KERR_NULL;

  return dir->ops->f_remove_child(dir, name);
}

struct oss_obj* dir_find(dir_t* dir, const char* path)
{
  if (!dir || !path)
    return nullptr;

  if (!dir->ops || !dir->ops->f_find)
    return nullptr;

  return dir->ops->f_find(dir, path);
}

int dir_read(dir_t* dir, uint64_t idx, direntry_t* bentry)
{
  if (!dir)
    return -KERR_INVAL;

  if (!dir->ops || !dir->ops->f_read)
    return -KERR_INVAL;

  return dir->ops->f_read(dir, idx, bentry);
}

dir_t* dir_open(const char* path)
{
  dir_t* ret;
  oss_node_t* node = NULL;

  oss_resolve_node(path, &node);

  if (!node)
    return nullptr;

  ret = node->dir;

  /* This node does not have a directory, so we'll probably have to query the gen node */
  if (!ret) {
    do {
      node = node->parent;
    } while (node && node->type != OSS_OBJ_GEN_NODE);

    if (!node)
      return nullptr;

    uintptr_t idx = 0;
    const char* rel_path = nullptr;

    while (path[idx]) {
      /* We've found the node name in our path yay */
      if (strncmp(&path[idx], node->name, strlen(node->name)) == 0) {
        rel_path = &path[idx + strlen(node->name) + 1];
        break;
      }

      idx++;
    }

    /* Scan failed */
    if (!rel_path)
      return nullptr;

    if (!KERR_OK(oss_node_query_node(node, rel_path, &node)))
      return nullptr;

    ret = node->dir;
  }

  dir_ref(ret);

  return ret;
}

/*!
 * @brief: Close a directory
 *
 * All this pretty much needs to do is check if we can kill the directory and it's owned memory
 * and kill it if we can. Ownership of the directory lays with the associated oss_node, so we'll have
 * to look at that to see if we can murder. If it has no attached objects, we can proceed to kill it.
 * otherwise we'll just have to detach it's child and revert it's type back to a storage node
 */
kerror_t dir_close(dir_t* dir)
{
  dir_unref(dir);
  return 0;
}

kerror_t init_direntry(direntry_t* dirent, void* entry, enum DIRENT_TYPE type)
{
  if (!dirent || !entry)
    return -KERR_INVAL;

  dirent->_entry = entry;
  dirent->type = type;

  return 0;
}

kerror_t close_direntry(direntry_t* entry)
{
  if (!entry || !entry->_entry)
    return -KERR_INVAL;

  switch (entry->type) {
    case DIRENT_TYPE_FILE:
      file_close(entry->file);
      break;
    case DIRENT_TYPE_DIR:
      dir_close(entry->dir);
      break;
    default:
      kernel_panic("TODO: unhandled direntry type in close_direntry");
  }

  return 0;
}

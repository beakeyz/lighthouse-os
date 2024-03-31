#include "dir.h"
#include "dev/core.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "libk/string.h"
#include "sync/mutex.h"

/*!
 * @brief: Create a directory at the end of @path
 *
 * @root: The root node of the filesystem this directory comes from
 * @path: The path relative from @root
 */
dir_t* create_dir(oss_node_t* root, const char* path, struct dir_ops* ops, void* priv, uint32_t flags)
{
  dir_t* dir;
  const char* name;

  name = oss_get_objname(path);

  dir = kmalloc(sizeof(*dir));

  if (!dir)
    return nullptr;

  memset(dir, 0, sizeof(*dir));

  dir->node = oss_create_path(root, path);
  dir->name = name;
  dir->ops = ops;
  dir->priv = priv;
  dir->flags = flags;

  mutex_lock(dir->node->lock);

  dir->node->type = OSS_DIR_NODE;
  dir->node->priv = dir;

  mutex_unlock(dir->node->lock);
  
  return dir;
}

void destroy_dir(dir_t* dir)
{
  if (!dir)
    return;

  if (dir->ops && dir->ops->f_destroy)
    dir->ops->f_destroy(dir);

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

struct oss_obj* dir_read(dir_t* dir, uint64_t idx)
{
  if (!dir)
    return nullptr;

  if (!dir->ops || !dir->ops->f_read)
    return nullptr;

  return dir->ops->f_read(dir, idx);
}

dir_t* dir_open(const char* path)
{
  oss_node_t* node = NULL;
  oss_resolve_node(path, &node);

  if (!node || node->type != OSS_DIR_NODE)
    return nullptr;

  return node->priv;
}

kerror_t dir_close(dir_t* dir)
{
  kernel_panic("TODO: dir_close");
}


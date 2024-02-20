#include "core.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/node.h"
#include <sync/mutex.h>
#include <libk/stddef.h>
#include <libk/string.h>

static fs_type_t* fsystems;
static mutex_t* fsystems_lock;

static fs_type_t** find_fs_type(const char* name, size_t length) 
{
  fs_type_t** ret;

  for (ret = &fsystems; *ret; ret = &(*ret)->m_next) {
    /* Check if the names match and if ->m_name is a null-terminated string */
    if (memcmp((*ret)->m_name, name, length) && !(*ret)->m_name[length]) {
      break;
    }
  }

  return ret;
}

fs_type_t* get_fs_driver(fs_type_t* fs) 
{
  kernel_panic("TODO: implement get_fs_driver");
}

ErrorOrPtr register_filesystem(fs_type_t* fs) 
{
  fs_type_t** ptr;

  if (fs->m_next)
    return Error();

  mutex_lock(fsystems_lock);

  ptr = find_fs_type(fs->m_name, strlen(fs->m_name));

  if (*ptr) {
    return Error();
  } else {
    *ptr = fs;
  }

  mutex_unlock(fsystems_lock);

  return Success(0);
}

ErrorOrPtr unregister_filesystem(fs_type_t* fs) 
{
  kernel_panic("TODO: test unregister_filesystem");

  fs_type_t** itterator;

  mutex_lock(fsystems_lock);

  itterator = &fsystems;

  while (*itterator) {
    if (*itterator == fs) {

      *itterator = fs->m_next;
      fs->m_next = nullptr;
      
      mutex_unlock(fsystems_lock);
      return Success(0);
    }

    itterator = &(*itterator)->m_next;
  }

  mutex_unlock(fsystems_lock);
  return Error();
}

static fs_type_t* __get_fs_type(const char* name, uint32_t length) 
{
  fs_type_t* ret;

  mutex_lock(fsystems_lock);

  ret = *(find_fs_type(name, length));

  /* Don't dynamicaly load and the driver does not have to be active */
  if (ret && !try_driver_get(ret->m_driver, NULL)) {
    kernel_panic("Could not find driver");
    ret = NULL;
  }

  mutex_unlock(fsystems_lock);
  return ret;
}

fs_type_t* get_fs_type(const char* name) 
{
  fs_type_t* ret;

  ret = __get_fs_type(name, strlen(name));

  if (!ret) {
    // Can we do anything to still try to get this filesystem?
  }

  return ret;
}

struct oss_node* create_fs_oss_node(const char* name, fs_type_t* type, struct oss_node_ops* ops)
{
  oss_node_t* node;
  fs_oss_node_t* fsnode;

  if (!name || !ops)
    return nullptr;

  if (!ops->f_destroy)
    return nullptr;

  node = create_oss_node(name, OSS_OBJ_GEN_NODE, ops, NULL);

  if (!node)
    return nullptr;

  /* 
   * Only allocate a fs node. It's up to the caller
   * to populate the fields
   */
  fsnode = kmalloc(sizeof(*fsnode));

  if (!fsnode)
    goto dealloc_and_fail;

  memset(fsnode, 0, sizeof(*fsnode));

  fsnode->m_type = type;

  node->priv = fsnode;

  return node;

dealloc_and_fail:
  destroy_oss_node(node);
  return nullptr;
}

/*!
 * @brief: Destroy an oss node with an fsnode attached
 *
 * This assumes that the internals of the fsnode are already destroyed
 */
void destroy_fs_oss_node(struct oss_node* node)
{
  fs_oss_node_t* fsnode;

  fsnode = oss_node_getfs(node);

  if (!fsnode)
    return;

  kfree(fsnode);

  destroy_oss_node(node);
}

/*!
 * @brief: Initialize the filesystem core
 *
 * Here we initialize the oss interface for regular fs-use
 */
void init_fs_core() 
{
  fsystems_lock = create_mutex(0);
}


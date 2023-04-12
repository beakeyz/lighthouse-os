
#include "vnode.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/stddef.h>


/*
 * Some dummy op functions
 */

static int vnode_write(vnode_t* node, void* buffer, uintptr_t offset, size_t size) {
  return 0;
}

static int vnode_read(vnode_t* node, void* buffer, uintptr_t offset, size_t size) {
  return 0;
}

static int vnode_msg(vnode_t* node, driver_control_code_t code, void* buffer, size_t size) {
  return NULL;
}

static void* vnode_open_stream(vnode_t* node) {
  return NULL;
}

static void* vnode_close_stream(vnode_t* node) {
  return NULL;
}

vnode_t* create_generic_vnode(const char* name, uint32_t flags) {
  vnode_t* node;

  if (!name)
    return nullptr;

  node = kmalloc(sizeof(vnode_t));

  memset(node, 0x00, sizeof(vnode_t));

  node->f_write = vnode_write;
  node->f_read = vnode_read;
  node->f_msg = vnode_msg;

  node->m_name = name;
  node->m_id = -1;

  node->m_flags = flags;

  node->f_open_stream = vnode_open_stream;
  node->f_close_stream = vnode_close_stream;

  return node;
}

/* TODO: what do we do when we try to destroy a linked node? */
ErrorOrPtr destroy_generic_vnode(vnode_t* node) {

  if (!node || vn_seems_mounted(*node)) {
    return Error();
  }

  /*
   * If the device is not deattached, this node can't be destroyed
   */
  if (node->m_dev)
    return Error();

  /*
   * A locked node means that there is something trying to opperate 
   * on this node. It would be a bad idea to destroy it at this point...
   */
  if (mutex_is_locked(node->m_lock))
    return Error();

  /* Lock the mutex in order to prevent further opperations */
  mutex_lock(node->m_lock);

  /* TODO: clear node cache */
  if (node->m_flags & VN_CACHED) {
    kernel_panic("TODO: clean vnode from cache");
  }

  destroy_mutex(node->m_lock);

  kfree(node);

  return Success(0);
}

bool vn_seems_mounted(vnode_t node) {
  if ((node.m_flags & VN_MOUNT))
    return true;

  if (node.m_id > 0)
    return true;

  if (node.m_ns)
    return true;

  return false;
}

bool vn_is_link(vnode_t node) {
  return (node.m_flags & VN_LINK);
}

/*
 * Also prepare the write queues
 */
void vn_freeze(vnode_t* node) {
  node->m_flags |= VN_FROZEN;
}

/*
 * If writes are queued, we need to flush them...
 */
void vn_unfreeze(vnode_t* node) {
  node->m_flags &= ~VN_FROZEN;
}




#include "vnode.h"
#include "fs/vobj.h"
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
  node->m_lock = create_mutex(0);
  node->m_vobj_lock = create_mutex(0);

  node->m_name = kmalloc(strlen(name) + 1);
  memcpy((void*)node->m_name, name, strlen(name) + 1);

  node->m_id = -1;

  node->m_flags = flags;

//  node->f_open_stream = vnode_open_stream;
//  node->f_close_stream = vnode_close_stream;

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
  destroy_mutex(node->m_vobj_lock);

  kfree((void*)node->m_name);
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

int vn_take(vnode_t* node, uint32_t flags) {
  if (!node)
    return -1;

  if (mutex_is_locked(node->m_lock))
    return -1;

  /* Take the mutex */
  mutex_lock(node->m_lock);

  ASSERT_MSG(node->m_objects == nullptr, "vnode had objects attached as it was taken!");

  node->m_flags |= VN_TAKEN;

  if (flags & VN_SYS)
    node->m_flags |= VN_SYS;
  if (flags & VN_FROZEN)
    vn_freeze(node);

  return 0;
}

int vn_release(vnode_t* node) {
  /* Can't release someone elses node =D */
  if (!node || !mutex_is_locked_by_current_thread(node->m_lock))
    return -1;

  mutex_unlock(node->m_lock);

  vobj_t** obj = &node->m_objects;

  while (*obj) {
    ASSERT_MSG((*obj)->m_ops, "vnode contained invalid vobject!");

    /* Cache the next */
    vobj_t** next = &(*obj)->m_next;

    vn_detach_object(node, *obj);

    /* Murder the object */
    destroy_vobj(*obj);

    obj = next;
  }

  node->m_flags &= ~VN_TAKEN;

  return 0;
}

static vobj_t** find_vobject(vnode_t* node, vobj_t* obj) {
  if (!node || !obj)
    return nullptr;

  vobj_t** ret = &node->m_objects;

  for (; *ret; ret = &(*ret)->m_next) {
    if (*ret == obj)
      break;
  }

  return ret;
}

static vobj_t** find_vobject_path(vnode_t* node, const char* path) {
  if (!node || !path)
    return nullptr;

  vobj_t** ret = &node->m_objects;

  for (; *ret; ret = &(*ret)->m_next) {
    if (memcmp((*ret)->m_path, path, strlen(path)))
      break;
  }

  return ret;
}

ErrorOrPtr vn_attach_object(vnode_t* node, struct vobj* obj) {
  if (!node || !obj)
    return Error();

  /* Only allow attaching when the node is taken */
  if ((node->m_flags & VN_TAKEN) == 0) 
    return Error();

  /* TODO: we could try moving the object */
  if (obj->m_parent) {
    if (obj->m_flags & VOBJ_MOVABLE) {
      kernel_panic("TODO: this vobject is movable. IMPLEMENT");
    }
    return Error();
  }

  mutex_lock(node->m_vobj_lock);

  vobj_t** slot = find_vobject(node, obj);

  if (*slot)
    goto exit_error;

  *slot = obj;

  obj->m_parent = node;

exit_success:;
  mutex_unlock(node->m_vobj_lock);
  return Success(0);

exit_error:;
  mutex_unlock(node->m_vobj_lock);
  return Error();
}

/*
 * TODO: do we care if the node is taken or not at this point?
 */
ErrorOrPtr vn_detach_object(vnode_t* node, struct vobj* obj) {

  if (!node || !obj)
    return Error();

  if (!obj->m_parent)
    return Error();

  mutex_lock(node->m_vobj_lock);

  vobj_t** slot = find_vobject(node, obj);

  if (*slot) {
    /* Sanity */
    ASSERT_MSG(*slot == obj, "vobj: weird mismatch while detaching from vnode");

    *slot = obj->m_next;
    obj->m_next = nullptr;

    mutex_unlock(node->m_vobj_lock);
    return Success(0);
  }

  mutex_unlock(node->m_vobj_lock);
  return Error();
}

ErrorOrPtr vn_detach_object_path(vnode_t* node, const char* path) {

  vobj_t** slot = find_vobject_path(node, path);

  if (!slot)
    return Error();

  if (!*slot)
    return Error();

  return vn_detach_object(node, *slot);
}

bool vn_has_object(vnode_t* node, const char* path) {
  vobj_t** obj = find_vobject_path(node, path);

  return (obj && *obj);
}

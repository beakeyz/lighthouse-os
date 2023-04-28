
#include "vnode.h"
#include "dev/debug/serial.h"
#include "fs/vobj.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
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
    vobj_t* current = *obj;

    ASSERT_MSG(current->m_ops, "vnode contained invalid vobject!");

    /* Cache the next */
    vobj_t** next = &current->m_next;

    /* NOTE: Too aggressive must here */
    Must(vn_detach_object(node, current));

    /* Murder the object */
    destroy_vobj(current);

    obj = next;
  }

  node->m_objects = nullptr;
  node->m_flags &= ~VN_TAKEN;

  return 0;
}

static vobj_t** find_vobject(vnode_t* node, vobj_t* obj) {
  if (!node || !obj)
    return nullptr;

  vobj_t** ret = &node->m_objects;

  /* Loop until we match pointers */
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

  /* Loop until there is no next object */
  for (; *ret; ret = &(*ret)->m_next) {
    /* Check path */
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
    println("Has parent");
    return Error();
  }

  /* vobject opperations are locked by m_vobj_lock, so we need to take it here */
  mutex_lock(node->m_vobj_lock);

  /* Look for the thing */
  vobj_t** slot = find_vobject(node, obj);

  /* Slot already taken?? */
  if (*slot)
    goto exit_error;

  /* Assign slot */
  *slot = obj;

  /* Take ownership of the object */
  obj->m_parent = node;

  /* Increment count */
  node->m_object_count++;

exit_success:
  println("Generic fail");
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

  /* Can't detach an object if there are none on the node -_- */
  if (node->m_object_count == 0) {
    return Error();
  }

  mutex_lock(node->m_vobj_lock);

  vobj_t** slot = find_vobject(node, obj);

  if (*slot) {
    /* Sanity */
    ASSERT_MSG(*slot == obj, "vobj: weird mismatch while detaching from vnode");

    *slot = obj->m_next;
    obj->m_next = nullptr;

    node->m_object_count--;

    mutex_unlock(node->m_vobj_lock);
    return Success(0);
  }

  mutex_unlock(node->m_vobj_lock);
  return Error();
}

ErrorOrPtr vn_detach_object_path(vnode_t* node, const char* path) {

  /* First find the object without taking the mutex */
  vobj_t** slot = find_vobject_path(node, path);

  if (!slot)
    return Error();

  if (!*slot)
    return Error();

  /* Then detach while taking the mutex */
  return vn_detach_object(node, *slot);
}

bool vn_has_object(vnode_t* node, const char* path) {
  vobj_t** obj = find_vobject_path(node, path);

  return (obj && *obj);
}

struct vobj* vn_get_object(vnode_t* node, const char* path) {
  
  vobj_t* itt;

  if (!node || !path)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  itt = node->m_objects;

  while (itt) {

    if (memcmp(itt->m_path, path, strlen(path))) {
      mutex_unlock(node->m_vobj_lock);
      return itt;
    }

    itt = itt->m_next;
  }

  mutex_unlock(node->m_vobj_lock);
  return nullptr;
}

struct vobj* vn_get_object_idx(vnode_t* node, uintptr_t idx) {

  vobj_t* itt;

  if (!node)
    return nullptr;

  if (idx == 0) {
    return node->m_objects;
  }

  mutex_lock(node->m_vobj_lock);

  itt = node->m_objects;

  for (uintptr_t i = 0; i < idx; i++) {
    /* Could not find it (index > object count) */
    if (!itt) {
      mutex_unlock(node->m_vobj_lock);
      return nullptr;
    }

    itt = itt->m_next;
  }

  mutex_unlock(node->m_vobj_lock);
  return itt;
}

ErrorOrPtr vn_obj_get_index(vnode_t* node, vobj_t* obj) {

  ErrorOrPtr result;
  uintptr_t idx;
  vobj_t** itt;

  if (!node)
    return Error();

  itt = &node->m_objects;
  result = Error();
  idx = 0;

  while (*itt) {

    if (*itt == obj) {
      result = Success(idx);
      break;
    }

    itt = &(*itt)->m_next;
    idx++;
  }

  return result;
}

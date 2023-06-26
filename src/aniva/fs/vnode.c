
#include "vnode.h"
#include "dev/debug/serial.h"
#include "fs/vobj.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

static vobj_t** find_vobject_path(vnode_t* node, const char* path);

/*
 * Some dummy op functions
 */
static int vnode_write(vnode_t* node, void* buffer, uintptr_t offset, size_t size) {
  return -1;
}

static int vnode_read(vnode_t* node, void* buffer, uintptr_t offset, size_t size) {
  return -1;
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
  
static vobj_t* vnode_find(vnode_t* node, char* name) {

  if (!node || !name)
    return nullptr;

  if (!vn_is_available(node) || !node->m_objects)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  vobj_t* ret;
  vobj_t** slot = find_vobject_path(node, name);

  if (slot && *slot) {
    ret = *slot;

    mutex_unlock(node->m_vobj_lock);
    return ret;
  }

  mutex_unlock(node->m_vobj_lock);
  return nullptr;
}

static int vnode_force_sync(vnode_t* node) {
  return -1;
}

static struct generic_vnode_ops __generic_vnode_ops = {
  .f_write = vnode_write,
  .f_read = vnode_read,
  .f_force_sync = vnode_force_sync,
  .f_msg = vnode_msg,
  .f_find = vnode_find,
};

vnode_t* create_generic_vnode(const char* name, uint32_t flags) {
  vnode_t* node;

  if (!name)
    return nullptr;

  node = kmalloc(sizeof(vnode_t));

  memset(node, 0x00, sizeof(vnode_t));

  node->m_ops = &__generic_vnode_ops;
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
  return (node.m_flags & VN_LINK) == VN_LINK;
}

/*
 * Also prepare the write queues
 */
void vn_freeze(vnode_t* node) {
  node->m_flags |= VN_FROZEN;
}

bool vn_is_available(vnode_t* node) {

  if ((node->m_flags & VN_TAKEN) == VN_TAKEN) {
    return (mutex_is_locked_by_current_thread(node->m_lock));
  }
  
  /*
   * If the vnode is not taken by the current thread, the only
   * way it can be available is if it is marked flexible
   */
  return (node->m_flags & VN_FLEXIBLE) == VN_FLEXIBLE;
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
  if (!node || (node->m_flags & VN_TAKEN) == 0 || !mutex_is_locked_by_current_thread(node->m_lock))
    return -1;

  vobj_t** obj = &node->m_objects;

  while (*obj) {
    vobj_t* current = *obj;

    ASSERT_MSG(current->m_ops, "vnode contained invalid vobject!");

    /* Cache the next */
    vobj_t** next = &current->m_next;

    /* Murder the object and detach it */
    destroy_vobj(current);

    obj = next;
  }

  node->m_objects = nullptr;
  node->m_flags &= ~VN_TAKEN;

  mutex_unlock(node->m_lock);

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

  /* Only allow attaching when the node is taken or flexible */
  if (!vn_is_available(node)) 
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

  /* Try to generate handle */
  TRY(gen_res, vobj_generate_handle(obj));

  /* Assign the handle */
  obj->m_handle = gen_res;

exit_success:
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
    obj->m_handle = 0;

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

/*
 * Loops through the nodes links and tries to find a link
 * with the name of link_name. Returns true (1) if it finds
 * one, returns false (0) if it does not
 * 
 * returns false if an error occured
 */
static bool vn_has_link(vnode_t* node, const char* link_name) {

  if (!node || link_name)
    return false;

  if ((node->m_flags & VN_LINK) == 0)
    return false;

  FOREACH(i, node->m_links) {
    vnode_t* link = i->data;

    if (memcmp(link->m_name, link_name, strlen(link_name))) {
      return true;
    }
  }

  return false;
}

ErrorOrPtr vn_link(vnode_t* node, vnode_t* link) {
  if (!node || !link)
    return Error();

  if (vn_has_link(node, link->m_name))
    return Error();

  /* First time we link a node, so initialize */
  if ((node->m_flags & VN_LINK) == 0) {
    if (node->m_ref) {
      /* No link, but this node is referenced. */
      return Error();
    }

    node->m_links = init_list();
  }

  list_append(node->m_links, link);

  return Success(0);
}

ErrorOrPtr vn_unlink(vnode_t* node, vnode_t* link) {
  if (!node || !link)
    return Error();

  if (!vn_has_link(node, link->m_name))
    return Error();

  TRY(result, list_indexof(node->m_links, link));

  uint32_t index = result;

  bool remove_res = list_remove(node->m_links, index);

  if (!remove_res)
    return Error();

  /* This was the last link, mark the node as unlinked */
  if (!node->m_links->m_length) {
    node->m_flags &= ~VN_LINK;
    destroy_list(node->m_links);
  }

  return Success(0);
}

vnode_t* vn_get_link(vnode_t* node, const char* link_name) {
  if (!node || !link_name)
    return nullptr;

  if ((node->m_flags & VN_LINK) == 0 || !node->m_links)
    return nullptr;

  FOREACH(i, node->m_links) {
    vnode_t* link = i->data;

    if (memcmp(link->m_name, link_name, strlen(link_name))) {
      return link;
    }
  }

  return nullptr;
}

/*
 * Wrapper around ->f_find
 */
vobj_t* vn_find(vnode_t* node, char* name) {

  vobj_t* ret;

  if (!node || !name)
    return nullptr;

  ret = vn_get_object(node, name);

  if (ret)
    return ret;

  ret = node->m_ops->f_find(node, name);

  return ret;
}


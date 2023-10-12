
#include "vnode.h"
#include "dev/debug/serial.h"
#include "fs/vfs.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/reference.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include <libk/stddef.h>
#include "vdir.h"

/*
 * Find the last VFS_PATH_SEPERATOR and cut it off
 */
static void __cut_off_filename(char* path)
{
  uint64_t i = strlen(path) - 1;

  while (i--) {
    if (path[i] == VFS_PATH_SEPERATOR) {
      path[i] = NULL;
      break;
    }
  }
}

/* 
 * This always takes an absolute path
 */
static vdir_t* __scan_for_dir_from(vdir_t* root, const char* path)
{
  vdir_t* prev;
  vdir_t* ret;
  bool reached_end;
  char* buffer;
  char* buffer_start;

  if (!root)
    return nullptr;

  /* Quick copy of the path on the heap ;-; */
  buffer = strdup(path);
  buffer_start = buffer;
  reached_end = false;
  ret = root;
  prev = root;

  /* Skip the roots name in the scan */
  if (memcmp(root->m_name, path, root->m_name_len))
    buffer += root->m_name_len;

  /* Buffer will be freed inside this loop */
  while (buffer && !reached_end) {
    
    while (*buffer != VFS_PATH_SEPERATOR) {
      if (*buffer == NULL) {
        reached_end = true;
        break;
      }
      buffer++;
    }

    /* Set */
    *buffer = NULL;

    for (; ret; ret = ret->m_next_sibling) {
      if (memcmp(buffer_start, ret->m_name, ret->m_name_len)) {
        break;
      }
    }

    if (!ret || reached_end) {
      buffer = nullptr;
      break;
    } else {
      /* Decend into the next subdir */
      prev = ret;
      ret = ret->m_subdirs;
    }
    /* Reset */
    *buffer = VFS_PATH_SEPERATOR;
    buffer++;
  }

  kfree(buffer_start);

  /* Did we reach the end? */
  if (buffer)
    return nullptr;

  if (ret)
    return ret;

  return prev;
}

static vdir_t* __create_vobject_path(vnode_t* node, const char* path) 
{
  vdir_t* ret;
  vdir_t* prev;
  bool reached_end;
  char* buffer;
  char* buffer_start;

  /* Quick copy of the path on the heap ;-; */
  buffer = strdup(path);
  buffer_start = buffer;
  reached_end = false;
  ret = node->m_root_vdir->m_subdirs;
  prev = node->m_root_vdir;

  /* Buffer will be freed inside this loop */
  while (buffer && !reached_end) {
    
    while (*buffer != VFS_PATH_SEPERATOR) {
      if (*buffer == NULL) {
        reached_end = true;
        break;
      }
      buffer++;
    }

    /* Set */
    *buffer = NULL;

    println("buffer_start");
    println(buffer_start);

    for (; ret; ret = ret->m_next_sibling) {
      println(ret->m_name);
      if (memcmp(buffer_start, ret->m_name, ret->m_name_len)) {
        break;
      }
    }

    if (reached_end) {
      buffer = nullptr;
      break;
    } 

    if (!ret) {
      /* Create a vdir and link it */
      ret = create_vdir(node, prev, buffer_start);
    } else {
      /* Decend into the next subdir */
      prev = ret;
      ret = ret->m_subdirs;
    }
    /* Reset */
    *buffer = VFS_PATH_SEPERATOR;
    buffer++;
  }

  kfree(buffer_start);

  /* Assert? */
  if (buffer)
    return nullptr;

  if (!ret)
    return prev;

  return ret;
}

static vobj_t* __find_vobject_path(vdir_t* root, const char* path) 
{
  vobj_t* ret;
  vdir_t* dir;

  if (!root || !path)
    return nullptr;

  dir = __scan_for_dir_from(root, path);

  if (!dir)
    return nullptr;

  /* Loop until there is no next object */
  for (ret = dir->m_objects; ret; ret = ret->m_next) {
    /* Check path */
    if (memcmp(ret->m_path, path, strlen(path)))
      break;
  }

  return ret;
}

/*
 * Some dummy op functions
 */

static int vnode_msg(vnode_t* node, driver_control_code_t code, void* buffer, size_t size) 
{
  return NULL;
}

/*
 * A stream is a glorified buffer that can take in and store data and notify
 * stream holders of data changes, since the stream holds pointers to notification
 * functions it calls when there is data IO in the stream
static void* vnode_open_stream(vnode_t* node, void* stream, size_t stream_size) 
{
  return NULL;
}

static void* vnode_close_stream(vnode_t* node, void* stream) 
{
  return NULL;
}
*/
  
/*!
 * @brief: Default vnode open function
 *
 * Tries to find a vobject in the vdir chain
 */
static vobj_t* vnode_open(vnode_t* node, char* name) 
{

  if (!node || !name)
    return nullptr;

  if (!vn_is_available(node) || !node->m_root_vdir)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  vobj_t* ret = __find_vobject_path(node->m_root_vdir, name);

  mutex_unlock(node->m_vobj_lock);
  return ret;
}

static int vnode_close(vnode_t* node, vobj_t* obj)
{
  return 0;
}

static int vnode_force_sync(vnode_t* node) 
{
  return -1;
}

static int vnode_force_sync_once(vnode_t* node, vobj_t* obj) 
{
  return -1;
}

static int vnode_makedir(vnode_t* node, void* dir_entry, void* dir_attr, uint32_t flags)
{
  return -1;
}

static int vnode_rmdir(vnode_t* node, void* dir_entry, uint32_t flags)
{
  return -1;
}

static struct generic_vnode_ops __generic_vnode_ops = {
  .f_force_sync = vnode_force_sync,
  .f_force_sync_once = vnode_force_sync_once,
  .f_msg = vnode_msg,
  .f_open = vnode_open,
  .f_close = vnode_close,
  .f_makedir = vnode_makedir,
  .f_rmdir = vnode_rmdir,
};

/*
 * FIXME: When creating a vnode, we really should require the caller to also 
 * set a type every time...
 */
vnode_t* create_generic_vnode(const char* name, uint32_t flags) {
  vnode_t* node;

  if (!name)
    return nullptr;

  node = kmalloc(sizeof(vnode_t));

  memset(node, 0x00, sizeof(vnode_t));

  node->m_ops = &__generic_vnode_ops;
  node->m_lock = create_mutex(0);
  node->m_vobj_lock = create_mutex(0);

  node->m_name = strdup(name);
  node->m_id = -1;
  node->m_flags = flags;

  /* Take 2 pages initially to store any vdir objects */
  node->m_vdir_allocator = create_zone_allocator(8 * Kib, sizeof(vdir_t), NULL);

  /* Create the root vdir for this node */
  node->m_root_vdir = create_vdir(node, NULL, node->m_name);

  vnode_set_type(node, VNODE_NONE);

  if ((flags & VN_FS) == VN_FS)
    vnode_set_type(node, VNODE_MOUNTPOINT);

  return node;
}

/* TODO: what do we do when we try to destroy a linked node? */
ErrorOrPtr destroy_generic_vnode(vnode_t* node) 
{
  println("Destroying thing");
  if (!node || vn_seems_mounted(*node))
    return Error();

  /*
   * If the node has a driver that is not deattached, this node can't be destroyed
   */
  if (vnode_has_driver(node))
    return Error();

  /* TODO: clear node cache */
  if (node->m_flags & VN_CACHED) {
    kernel_panic("TODO: clean vnode from cache");
  }

  /* Kill all the directories and the objects that are attached to them */
  if (node->m_root_vdir)
    destroy_vdirs(node->m_root_vdir, true);

  if (node->m_vdir_allocator)
    destroy_zone_allocator(node->m_vdir_allocator, true);

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

/*
 * If writes are queued, we need to flush them...
 */
void vn_unfreeze(vnode_t* node) {
  node->m_flags &= ~VN_FROZEN;
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

int vn_take(vnode_t* node, uint32_t flags) 
{
  if (!node)
    return -1;

  /*
   * If the caller OR the vnode wishes to deny waiting, we do
   */
  if (mutex_is_locked(node->m_lock) && ((flags & VN_NO_WAIT) == VN_NO_WAIT || (node->m_flags & VN_NO_WAIT) == VN_NO_WAIT))
    return -1;

  /* Take the mutex */
  mutex_lock(node->m_lock);

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

  /* TODO: should we destroy vdirs? */
  //destroy_vdirs(node->m_objects, true);

  node->m_flags &= ~VN_TAKEN;

  mutex_unlock(node->m_lock);

  return 0;
}
  
/*!
 * @brief Attach a vobject to a vnode
 *
 * Nothing to add here...
 */
ErrorOrPtr vn_attach_object(vnode_t* node, struct vobj* obj) 
{
  return vn_attach_object_rel(node, node->m_root_vdir, obj);
}

/*!
 * @brief Attach a vobject to a vdir inside a vnode
 *
 * Nothing to add here...
 */
ErrorOrPtr vn_attach_object_rel(vnode_t* node, struct vdir* dir, struct vobj* obj)
{
  vdir_t* slot;
  int result;

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
    //println("Has parent");
    return Error();
  }

  /* vobject opperations are locked by m_vobj_lock, so we need to take it here */
  mutex_lock(node->m_vobj_lock);

  println(obj->m_path);

  /* Look for the thing */
  slot = __create_vobject_path(node, obj->m_path);

  if (!slot)
    goto exit_error;

  result = vdir_put_vobj(slot, obj);

  if (result < 0)
    goto exit_error;

  ASSERT_MSG(obj->m_parent_dir == slot, "Parent dir mismatch for vobject attaching");

  /* Take ownership of the object */
  obj->m_parent = node;

  /* Increment count */
  node->m_object_count++;

  /* Try to generate handle */
  TRY(gen_res, vobj_generate_handle(obj));

  /* Assign the handle */
  obj->m_handle = gen_res;

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

  char* path;
  vdir_t* dir;
  vobj_t** slot;

  if (!node || !obj)
    return Error();

  if (!obj->m_parent)
    return Error();

  /* Can't detach an object if there are none on the node -_- */
  if (node->m_object_count == 0) {
    return Error();
  }

  path = strdup(obj->m_path);

  __cut_off_filename(path);

  mutex_lock(node->m_vobj_lock);

  /* Find the directory we are located in */
  dir = __scan_for_dir_from(node->m_root_vdir, path);

  if (!dir)
    goto error_and_exit;

  /* Check if it contains our slot */
  for (slot = &dir->m_objects; *slot; slot = &(*slot)->m_next) {
    if (*slot == obj)
      break;
  }

  if (*slot) {
    /* Sanity */
    ASSERT_MSG(*slot == obj, "vobj: weird mismatch while detaching from vnode");

    *slot = obj->m_next;

    obj->m_next = nullptr;
    obj->m_handle = 0;

    node->m_object_count--;

    kfree((void*)path);
    mutex_unlock(node->m_vobj_lock);
    return Success(0);
  }

error_and_exit:
  kfree((void*)path);
  mutex_unlock(node->m_vobj_lock);
  return Error();
}

ErrorOrPtr vn_detach_object_path(vnode_t* node, const char* path) {

  /* First find the object without taking the mutex */
  vobj_t* slot = __find_vobject_path(node->m_root_vdir, path);

  if (!slot)
    return Error();

  /* Then detach while taking the mutex */
  return vn_detach_object(node, slot);
}

bool vn_has_object(vnode_t* node, const char* path) {
  vobj_t* obj = __find_vobject_path(node->m_root_vdir, path);

  return (obj != nullptr);;
}

struct vobj* vn_get_object(vnode_t* node, const char* path) {
  
  vobj_t* ret;

  if (!node || !path)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  ret = __find_vobject_path(node->m_root_vdir, path);

  mutex_unlock(node->m_vobj_lock);
  return ret;
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
vobj_t* vn_open(vnode_t* node, char* name) {

  vobj_t* ret;

  if (!node || !name)
    return nullptr;

  if (!vn_is_available(node))
    return nullptr;

  /* Look if we have it cached */
  ret = vn_get_object(node, name);

  if (ret) {
    vobj_ref(ret);
    return ret;
  }

  ret = node->m_ops->f_open(node, name);

  vobj_ref(ret);

  return ret;
}

/*!
 * @brief Try to close a vobject on a vnode
 *
 * NOTE: handling of errors generated by this function is very important,
 * since not doing so may ignore the take/release semantics we have in place
 * to ensure save vnode usage
 */
int vn_close(vnode_t* node, struct vobj* obj)
{
  if (!node || !obj || !node->m_ops || !node->m_ops->f_close)
    return -1;

  if (!vn_is_available(node))
    return -2;

  /* Unref in order to only destroy when the object is not used anywhere */
  return vobj_unref(obj);
}

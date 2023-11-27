#include "vobj.h"
#include "dev/debug/serial.h"
#include "dev/device.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <crypto/k_crc32.h>

vobj_ops_t generic_ops = {
  .f_create = create_generic_vobj,
  .f_destroy = nullptr,
};

vobj_t* create_generic_vobj(vnode_t* parent, const char* path) 
{
  /* Vobjects can't have empty parents */
  if (!parent)
    return nullptr;

  /* A parent that is not taken should not have any object registered to it */
  if (!vn_is_available(parent))
    return nullptr;

  vobj_t* obj = kmalloc(sizeof(vobj_t));

  memset(obj, 0, sizeof(vobj_t));

  obj->m_lock = create_mutex(0);
  obj->m_ops = &generic_ops;

  obj->m_parent = parent;
  obj->m_child = nullptr;
  obj->m_type = VOBJ_TYPE_EMPTY;

  obj->m_path = strdup(path);

  /* This is quite aggressive, we should prob just clean and return nullptr... */
  //Must(vn_attach_object(parent, obj));

  return obj;
}

/*!
 * @brief: Clear the memory used by a vobject
 *
 * The caller should not take the objects mutex, since any child destruction function
 * might try to take the mutex
 */
void destroy_vobj(vobj_t* obj) 
{
  if (!obj)
    return;

  ASSERT_MSG(!mutex_is_locked(obj->m_lock), "Failed to destroy vobject! Its mutex is still held");
  ASSERT_MSG(obj->m_child && obj->f_destory_child, "No way to destroy child of vobj!");

  /* Try to detach */
  if (obj->m_parent)
    vn_detach_object(obj->m_parent, obj);

  /* Murder the child (wtf) */
  if (obj->f_destory_child)
    obj->f_destory_child(obj->m_child);

  /* Murder object */
  if (obj->m_ops && obj->m_ops->f_destroy)
    obj->m_ops->f_destroy(obj);

  destroy_mutex(obj->m_lock);

  kfree((void*)obj->m_path);
  kfree(obj);
}

void vobj_register_child(vobj_t* obj, void* child, VOBJ_TYPE_t type, FuncPtr destroy_fn)
{
  if (!obj)
    return;

  obj->f_destory_child = destroy_fn;
  obj->m_child = child;
  obj->m_type = type;
}

ErrorOrPtr vobj_generate_handle(vobj_t* object) {

  vobj_handle_t ret;
  uint32_t crc;

  if (!object)
    return Error();

  /* We need a handle of zero to create a handle and a parent */
  if (object->m_handle || !object->m_parent)
    return Error();

  crc = kcrc32(object, sizeof(vobj_t));
  ret = crc + object->m_parent->m_object_count;

  return Success(ret);
}

void vobj_ref(vobj_t* obj) 
{
  if (!obj)
    return;

  mutex_lock(obj->m_lock);
  flat_ref(&obj->m_refc);
  mutex_unlock(obj->m_lock);
}

int vobj_unref(vobj_t* obj)
{
  int error;
  vnode_t* parent;

  if (!obj)
    return -1;

  mutex_lock(obj->m_lock);
  flat_unref(&obj->m_refc);
  mutex_unlock(obj->m_lock);

  /* If the object is still referenced, we need not kill it yet */
  if (obj->m_refc)
    return 0;

  mutex_lock(obj->m_lock);

  parent = obj->m_parent;

  /* 
   * First, make the vnode (most likely filesystem) close the object 
   * This may not touch any of the vobj internal fields
   */
  error = parent->m_ops->f_close(parent, obj);

  mutex_unlock(obj->m_lock);

  if (error)
    return error;

  /* Second, kill the object itself */
  destroy_vobj(obj);

  return 0;
}

file_t* vobj_get_file(vobj_t* obj) 
{
  if (!obj)
    return nullptr;

  if (obj->m_type != VOBJ_TYPE_FILE)
    return nullptr;

  return (file_t*)obj->m_child;
}

device_t* vobj_get_device(vobj_t* obj) 
{
  if (!obj)
    return nullptr;

  if (obj->m_type != VOBJ_TYPE_DEVICE)
    return nullptr;

  return (device_t*)obj->m_child;
}

int vobj_close(vobj_t* obj)
{
  if (!obj || !obj->m_parent)
    return -1;

  return vn_close(obj->m_parent, obj);
}

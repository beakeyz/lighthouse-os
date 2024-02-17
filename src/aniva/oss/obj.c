#include "obj.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/string.h>

oss_obj_t* create_oss_obj(const char* name)
{
  oss_obj_t* ret;

  if (!name)
    return nullptr;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->type = OSS_OBJ_TYPE_EMPTY;
  ret->name = strdup(name);
  ret->parent = nullptr;
  ret->lock = create_mutex(NULL);

  init_atomic_ptr(&ret->refc, 1);

  return ret;
}

void destroy_oss_obj(oss_obj_t* obj)
{
  void* priv;
  oss_node_entry_t* entry = NULL;

  /* If we are still attached, might as well remove ourselves */
  if (obj->parent)
    oss_node_remove_entry(obj->parent, obj->name, &entry);

  /* Also destroy it's entry xD */
  if (entry)
    destroy_oss_node_entry(entry);

  /* Cache the private attachment */
  priv = obj->priv;
  obj->priv = nullptr;

  if (obj->ops.f_destory_priv)
    obj->ops.f_destory_priv(priv);

  destroy_mutex(obj->lock);

  kfree((void*)obj->name);
  kfree(obj);
}

const char* oss_obj_get_fullpath(oss_obj_t* obj)
{
  char* ret;
  size_t c_strlen;
  oss_node_t* c_parent;

  c_parent = obj->parent;

  if (!c_parent)
    return obj->name;

  /* <name 1> / <name 2> <Nullbyte> */
  c_strlen = strlen(c_parent->name) + 1 + strlen(obj->name) + 1;
  ret = kmalloc(c_strlen);

  (void)ret;
  kernel_panic("TODO: oss_obj_get_fullpath");
}

void oss_obj_ref(oss_obj_t* obj)
{
  uint64_t val;

  mutex_lock(obj->lock);

  val = atomic_ptr_read(&obj->refc);
  atomic_ptr_write(&obj->refc, val+1);

  mutex_unlock(obj->lock);
}

void oss_obj_unref(oss_obj_t* obj)
{
  uint64_t val;

  mutex_lock(obj->lock);

  val = atomic_ptr_read(&obj->refc) - 1;

  /* No need to write if  */
  if (!val)
    goto destroy_obj;

  atomic_ptr_write(&obj->refc, val);

  mutex_unlock(obj->lock);
  return;

destroy_obj:
  destroy_oss_obj(obj);
}

/*!
 * @brief: Close a base object
 *
 * Called when any oss object is closed, regardless of underlying type. When the object
 * is not referenced anymore (TODO) it is automatically disposed of
 *
 * In the case of devices, drivers and such more permanent objects, they need to be present on
 * the oss tree regardless of if they are referenced or not. To combat their preemptive destruction,
 * we initialize them with a reference_count of one, to indicate they are 'referenced' by their
 * respective subsystem. When we want to destroy for example a device that has an oss_object attached
 * to it, the subsystem asks oss to dispose of the object, but if the object has a referencecount of > 1
 * the entire destruction opperation must fail, since persistent oss residents are essential to the 
 * function of the system.
 */
int oss_obj_close(oss_obj_t* obj)
{
  oss_obj_unref(obj);
  return 0;
}

/*!
 * @brief: Walk the parent chain of an object to find the root node
 */
struct oss_node* oss_obj_get_root_parent(oss_obj_t* obj)
{
  oss_node_t* c_node;

  c_node = obj->parent;

  while (c_node) {
    
    /* If there is no parent on an attached object, we have reached the root */
    if (!c_node->parent)
      return c_node;

    c_node = c_node->parent;
  }

  /* This should never happen lmao */
  return nullptr;
}

void oss_obj_register_child(oss_obj_t* obj, void* child, enum OSS_OBJ_TYPE type, FuncPtr destroy_fn)
{
  if (!obj)
    return;

  mutex_lock(obj->lock);

  switch (type) {
    case OSS_OBJ_TYPE_EMPTY:
      if (!obj->priv)
        goto exit_and_unlock;

      if (obj->ops.f_destory_priv)
        obj->ops.f_destory_priv(obj->priv);

      obj->ops.f_destory_priv = nullptr;
      obj->priv = nullptr;
      obj->type = type;
      break;
    default:
      obj->priv = child;
      obj->type = type;
      obj->ops.f_destory_priv = destroy_fn;
      break;
  }

exit_and_unlock:
  mutex_unlock(obj->lock);
}

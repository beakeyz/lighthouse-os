#include "obj.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/node.h"
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

  return ret;
}

void destroy_oss_obj(oss_obj_t* obj)
{
  oss_node_entry_t* entry = NULL;

  /* If we are still attached, might as well remove ourselves */
  if (obj->parent)
    oss_node_remove_entry(obj->parent, obj->name, &entry);

  /* Also destroy it's entry xD */
  if (entry)
    destroy_oss_node_entry(entry);

  if (obj->ops.f_destory_priv)
    obj->ops.f_destory_priv(obj->priv);

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

void oss_obj_ref(oss_obj_t* obj);
int oss_obj_unref(oss_obj_t* obj);

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

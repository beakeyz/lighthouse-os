#include "vobj.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "sync/mutex.h"

static void generic_destroy_vobj(vobj_t* obj) {

  destroy_mutex(obj->m_lock);

  kfree((void*)obj->m_path);
  kfree(obj);
}

vobj_ops_t generic_ops = {
  .f_destroy = generic_destroy_vobj,
  .f_create = create_generic_vobj,
  .f_destory_child = nullptr,
};

vobj_t* create_generic_vobj(vnode_t* parent, const char* path) {

  /* Vobjects can't have empty parents */
  if (!parent)
    return nullptr;

  /* A parent that is not taken should not have any object registered to it */
  if ((parent->m_flags & VN_TAKEN) == 0) {
    return nullptr;
  }

  vobj_t* obj = kmalloc(sizeof(vobj_t));

  obj->m_lock = create_mutex(0);
  obj->m_ops = &generic_ops;

  obj->m_child = nullptr;
  obj->m_flags = VOBJ_EMPTY;
  obj->m_path = kmalloc(strlen(path) + 1);
  memcpy((void*)obj->m_path, path, strlen(path) + 1);

  /* This is quite aggressive, we should prob just clean and return nullptr... */
  Must(vn_attach_object(parent, obj));

  return obj;
}

void destroy_vobj(vobj_t* obj) {
  if (!obj)
    return;

  ASSERT_MSG(obj->m_child && obj->m_ops->f_destory_child, "No way to destroy child of vobj!");
  ASSERT_MSG(obj->m_ops->f_destroy, "No way to destroy object!");

  /* Murder the child (wtf) */
  obj->m_ops->f_destory_child(obj->m_child);

  /* Murder object */
  obj->m_ops->f_destroy(obj);
}

#include "file.h"
#include "dev/debug/serial.h"
#include "fs/vobj.h"
#include "mem/heap.h"

int f_close(file_t* file) {

  return 0;
}

int f_read(file_t* file, void* buffer, size_t size, uintptr_t offset) {

  return 0;
}

int f_write(file_t* file, void* buffer, size_t size, uintptr_t offset) {

  return 0;
}

int f_resize(file_t* file, size_t new_size) {
  /*
   * TODO: if new_size > ->m_size then we need to enlarge the buffer
   * if new_size < ->m_size then we can simply shrink the size and 
   * free up the memory that that gives us
   */

  return 0;
}

int generic_f_sync(file_t* file) {
  return 1;
}

file_ops_t generic_file_ops = {
  .f_close = f_close,
  .f_sync = generic_f_sync,
  .f_read = f_read,
  .f_write = f_write,
  .f_resize = f_resize,
};

file_t* create_file(struct vnode* parent, uint32_t flags, const char* path) {

  file_t* ret = kmalloc(sizeof(file_t));

  ret->m_flags = flags;
  ret->m_obj = create_generic_vobj(parent, path);

  if (!ret->m_obj) {
    kfree(ret);
    return nullptr;
  }

  ret->m_obj->m_flags |= VOBJ_FILE;
  ret->m_obj->m_flags &= ~VOBJ_EMPTY;
  ret->m_obj->m_child = ret;
  ret->m_obj->m_ops->f_destory_child = (void (*)(void*))destroy_file;

  ret->m_ops = &generic_file_ops;

  ret->m_data = nullptr;
  ret->m_size = 0;

  return ret;
}

void destroy_file(file_t* file) {
  println("Destroyed file object");
  kfree(file);
}

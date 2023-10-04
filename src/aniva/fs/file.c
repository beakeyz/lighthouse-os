#include "file.h"
#include "dev/debug/serial.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/page_dir.h"
#include "sync/mutex.h"

file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags);

int f_close(file_t* file) {

  kernel_panic("TODO: implement f_close");

  return 0;
}

/*
 * The standard file read funciton will try to read from the files data 
 * buffer if that exsists, otherwise we will have to go to the device to read
 * from it
 */
int f_read(file_t* file, void* buffer, size_t* size, uintptr_t offset) {

  uintptr_t end_offset;
  uintptr_t read_offset;

  if (!buffer || !size)
    return -1;

  /* FIXME: we should still be able to read from files even if they have no buffer */
  if (!file || !file->m_buffer)
    return -1;

  end_offset = offset + *size;
  read_offset = (uintptr_t)file->m_buffer + offset;

  /* TODO: in this case, we could try to read from the device (filesystem) */
  if (offset > file->m_buffer_size) {
    *size = 0;
    return -1;
  }

  /* TODO: also in this case, we should read from the device (filesystem) */
  if (end_offset > file->m_buffer_size) {
    uintptr_t delta = end_offset - file->m_buffer_size;

    /* We'll just copy everything we can */
    *size -= delta;
  }

  memcpy(buffer, (void*)read_offset, *size);

  return 0;
}

int f_write(file_t* file, void* buffer, size_t* size, uintptr_t offset) {

  kernel_panic("Tried to write to a file! (TODO: implement)");

  return 0;
}

int f_resize(file_t* file, size_t new_size) {
  /*
   * TODO: if new_size > ->m_size then we need to enlarge the buffer
   * if new_size < ->m_size then we can simply shrink the size and 
   * free up the memory that that gives us
   */

  kernel_panic("TODO: implement f_resize");

  return 0;
}

/*
 * NOTE: This function may block for a while
 */
int generic_f_sync(file_t* file) {

  int result;
  vobj_t* object;
  vnode_t* parent_node;

  if (!file)
    return -1;

  object = file->m_obj;

  if (!object)
    return -1;

  parent_node = object->m_parent;

  if (!parent_node || !parent_node->m_ops || !parent_node->m_ops->f_force_sync_once)
    return -1;

  /* Just force the node to sync this object */
  result = parent_node->m_ops->f_force_sync_once(parent_node, file->m_obj);

  if (result) {
    kernel_panic("FIXME: Could not sync file! (Implement handler)");
  }

  result = parent_node->m_ops->f_force_sync(parent_node);

  if (result) {
    kernel_panic("FIXME: Failed to force sync! (Implement handler)");
  }

  return result;
}


file_ops_t generic_file_ops = {
  .f_close = f_close,
  .f_sync = generic_f_sync,
  .f_read = f_read,
  .f_write = f_write,
  .f_kmap = f_kmap,
  .f_resize = f_resize,
};

/*
 * NOTE: mapping a file means the mappings get the same flags, both
 * in the kernel map, and in the new map
 */
file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags) {
  
  file_t ret = {
    0
  };

  /*
   * FIXME: this function returns the right stuff, but works 
   * in an incorrect way.
   */

  kernel_panic("TODO: fix f_kmap");

  const size_t def_size = ALIGN_UP(size, SMALL_PAGE_SIZE);
  const size_t page_count = kmem_get_page_idx(def_size);

  /* NOTE: aggressive Must() */
  vaddr_t alloc_result = Must(__kmem_kernel_alloc_range(def_size, custom_flags, page_flags));

  paddr_t physical_base = kmem_to_phys(nullptr, alloc_result);

  if (!physical_base) {
    kernel_panic("Potential bug: kmem_to_phys returned NULL");
    return ret;
  }

  vaddr_t new_virt_base = kmem_map_range(dir->m_root, kmem_ensure_high_mapping(physical_base), physical_base, page_count, custom_flags, page_flags);
    
  ret.m_buffer_size = def_size;
  ret.m_buffer = (void*)new_virt_base;
  ret.m_buffer_offset = file->m_buffer_offset;
  ret.m_ops = &generic_file_ops;

  return ret;
}

file_t* create_file(struct vnode* parent, uint32_t flags, const char* path) {

  file_t* ret = kmalloc(sizeof(file_t));

  ret->m_flags = flags;
  ret->m_obj = create_generic_vobj(parent, path);

  if (!ret->m_obj) {
    kfree(ret);
    return nullptr;
  }

  vobj_register_child(ret->m_obj, ret, VOBJ_TYPE_FILE, destroy_file);

  ret->m_ops = &generic_file_ops;

  ret->m_buffer = nullptr;
  ret->m_buffer_size = 0;

  return ret;
}

void file_set_ops(file_t* file, file_ops_t* ops)
{
  if (!file || !ops)
    return;

  file->m_ops = ops;
}

void destroy_file(file_t* file) {
  println("Destroyed file object");
  kfree(file);
}

int file_read(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
  int error;
  vobj_t* file_obj;

  if (!file)
    return -1;

  file_obj = file->m_obj;

  if (!file_obj)
    return -2;

  mutex_lock(file_obj->m_lock);

  error = file->m_ops->f_read(file, buffer, size, offset);

  mutex_unlock(file_obj->m_lock);

  return error;
}
int file_write(file_t* file, void* buffer, size_t* size, uintptr_t offset);
int file_sync(file_t* file);

int file_close(file_t* file);

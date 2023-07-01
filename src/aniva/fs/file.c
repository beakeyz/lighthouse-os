#include "file.h"
#include "dev/debug/serial.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/page_dir.h"

file_t f_kmap(file_t* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags);

int f_close(file_t* file) {

  kernel_panic("TODO: implement f_close");

  return 0;
}

int f_read(file_t* file, void* buffer, size_t* size, uintptr_t offset) {

  uintptr_t end_offset;
  uintptr_t read_offset;

  if (!buffer || !size)
    return -1;

  end_offset = offset + *size;
  read_offset = (uintptr_t)file->m_data + offset;

  /* Can't read outside of the file */
  if (offset > file->m_size) {
    *size = 0;
    return -1;
  }

  if (end_offset > file->m_size) {
    uintptr_t delta = end_offset - file->m_size;

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
    
  ret.m_size = def_size;
  ret.m_data = (void*)new_virt_base;
  ret.m_offset = file->m_offset;
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

  ret->m_obj->m_type = VOBJ_TYPE_FILE;
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

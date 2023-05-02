#ifndef __ANIVA_FIL_IMPL__
#define __ANIVA_FIL_IMPL__
#include "dev/disk/shared.h"
#include "libk/error.h"
#include "mem/page_dir.h"
#include "sync/atomic_ptr.h"
#include <sync/mutex.h>

/*
 * A file is the most generic type of vobj.
 * it represents an fs entry that can hold any arbitrary data of any size.
 * dealing with files should mean no need to worry about the underlying
 * filesystem or device. Reading from a file should simply be the following steps:
 *  1) open the file (using a handle, or a path)
 *  2) read the data into a buffer r sm
 *  3) close the file to clean up
 * writing should then be this:
 *  1) open the file (same as above)
 *  2) write into a certain offset or line
 *  3) close the file to clean up and save changes to device
 *
 * Ez pz
 */

struct file;
struct vobj;
struct vnode;

typedef struct file_ops {
  int (*f_sync)     (struct file* file);
  int (*f_read)     (struct file* file, void* buffer, size_t size, uintptr_t offset);
  int (*f_write)    (struct file* file, void* buffer, size_t size, uintptr_t offset);
  /* Allocate a part of the file into a buffer and map that buffer to the specefied page dir */
  struct file (*f_kmap)      (struct file* file, page_dir_t* dir, size_t size, uint32_t custom_flags, uint32_t page_flags);
  /* 
   * Closing the file: 
   *  - We will detach from the parent vnode
   *  - We will sync if we can
   *  - Try to clean data buffers if there are any
   */
  int (*f_close)    (struct file* file);
  int (*f_resize)   (struct file* file, size_t new_size);
} file_ops_t;

/*
 * When this object is alive, we assume it (and its vnode) have already already opend
 */
typedef struct file {

  uint32_t m_flags;

  /* Every file has an 'offset' or 'address' for where it starts */
  disk_offset_t m_offset;

  /* How many 'chunks' this file encapsulates */
  uintptr_t m_scatter_count;

  /* The files parent object */
  struct vobj* m_obj;

  file_ops_t* m_ops;

  /* Pointer to the data buffer. TODO: make this easily managable */
  void* m_data;
  size_t m_size;

} __attribute__((aligned(4))) file_t;

file_t* create_file(struct vnode* parent, uint32_t flags, const char* path);
file_t* create_file_from_vobj(struct vobj* obj);
void destroy_file(file_t* file);

/* TODO: what will the syntax be? */
ErrorOrPtr file_open();

#endif // !__ANIVA_FIL_IMPL__

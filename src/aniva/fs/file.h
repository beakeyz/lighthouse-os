#ifndef __ANIVA_FIL_IMPL__
#define __ANIVA_FIL_IMPL__
#include "dev/disk/shared.h"
#include "libk/flow/error.h"
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
  int (*f_read)     (struct file* file, void* buffer, size_t* size, uintptr_t offset);
  int (*f_write)    (struct file* file, void* buffer, size_t* size, uintptr_t offset);
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

#define FILE_READONLY       (0x00000001)
#define FILE_ORPHAN         (0x00000002)

/*
 * NOTE: When this object is alive, we assume it (and its vnode) have already been opend
 * 
 * Every file has a data buffer that can hold a single piece of contiguous memory that reflects 
 * the files internal data. We can only hold ONE buffer of a single size at a time, so when we
 * want to access a different part of the file (on disk for example) we'll have to (sync and) switch out
 * the entire buffer. We could (TODO) try to cache different buffers and link them together
 */
typedef struct file {

  uint32_t m_flags;
  uint32_t m_res0;

  /* How many 'chunks' this file encapsulates */
  uintptr_t m_scatter_count;

  /* The files parent object */
  struct vobj* m_obj;

  file_ops_t* m_ops;

  void* m_private;

  /* Every file has an 'offset' or 'address' for where it starts */
  disk_offset_t m_buffer_offset;

  /* Pointer to the data buffer. TODO: make this easily managable */
  void* m_buffer;
  size_t m_buffer_size;
  size_t m_total_size;

} __attribute__((aligned(4))) file_t;

file_t* create_file(struct vnode* parent, uint32_t flags, const char* path);
file_t* create_file_from_vobj(struct vobj* obj);
void destroy_file(file_t* file);

void file_set_ops(file_t* file, file_ops_t* ops);

int file_read(file_t* file, void* buffer, size_t* size, uintptr_t offset);
int file_write(file_t* file, void* buffer, size_t* size, uintptr_t offset);
int file_sync(file_t* file);

int file_close(file_t* file);

#endif // !__ANIVA_FIL_IMPL__

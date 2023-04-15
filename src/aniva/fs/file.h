#ifndef __ANIVA_FIL_IMPL__
#define __ANIVA_FIL_IMPL__
#include "sync/atomic_ptr.h"
#include <sync/mutex.h>

struct file;
struct vnode;

/* TODO: ops */
typedef struct file {
  const char* m_path;

  struct vnode* m_node;

  mutex_t* m_lock;
  atomic_ptr_t* m_count;

  uint32_t m_flags;
  
} __attribute__((aligned(4))) file_t;

/*
 * Open the file and increment atomic access counter
 */
static ALWAYS_INLINE file_t* get_file(file_t* file) {
  if (!file)
    return nullptr;

  atomic_ptr_write(file->m_count, atomic_ptr_load(file->m_count) + 1);
  return file;
}

/*
 * Getter for the vnode of this file
 */
static ALWAYS_INLINE struct vnode* get_file_vnode(file_t* file) {
  if (!file)
    return nullptr;

  return file->m_node;
}

typedef struct fhandle {
  uint32_t m_size;
  uint8_t m_fhandle[];
} fhandle_t;

// TODO:

#endif // !__ANIVA_FIL_IMPL__

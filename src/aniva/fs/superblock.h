#ifndef __ANIVA_FS_SUPERBLOCK__
#define __ANIVA_FS_SUPERBLOCK__

/*
 * A superblock is a generic structure that can be implemented by any filesystem
 * and it tells the kernel different things about the filesystem like blocksize,
 * files, root, ect.
 */

#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct sb_ops;

typedef struct {

  struct sb_ops* m_ops;
  mutex_t* m_ops_lock;

  uint32_t m_blocksize;
  uintptr_t m_max_filesize;

  void* m_fs_specific_info;
  /* TODO: */

  
} fs_superblock_t;

typedef struct sb_ops {
  void (*destroy_fs_specific_info)(void*);
} sb_ops_t;

static fs_superblock_t* create_fs_superblock() {
  fs_superblock_t* ret = kmalloc(sizeof(fs_superblock_t));

  ret->m_ops_lock = create_mutex(0);

  return ret;
}

static void destroy_fs_superblock(fs_superblock_t* block) {

  if (block->m_ops && block->m_ops->destroy_fs_specific_info && block->m_fs_specific_info)
    block->m_ops->destroy_fs_specific_info(block->m_fs_specific_info);

  if (block->m_ops_lock)
    destroy_mutex(block->m_ops_lock);

  kfree(block);
}

#endif // !__ANIVA_FS_SUPERBLOCK__

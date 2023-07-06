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
struct vnode;

typedef struct fs_superblock {

  struct sb_ops* m_ops;
  mutex_t* m_ops_lock;

  uint32_t m_blocksize;
  uint32_t m_flags;

  /* FIXME: are these fields supposed to to be 64-bit? */
  uint32_t m_dirty_blocks;
  uint32_t m_faulty_blocks;
  size_t m_free_blocks;
  size_t m_total_blocks;

  uintptr_t m_first_usable_block;

  uintptr_t m_max_filesize;

  void* m_fs_specific_info;
  
  /*
  size_t m_block_count;
  size_t m_reserved_block_count;
  uintptr_t m_last_usable_block;
  */

} fs_superblock_t;

typedef struct sb_ops {
  void (*destroy_fs_specific_info)(void*);
  int (*reload_sb)(struct vnode*);
  void (*remount_fs)(struct vnode*);
} sb_ops_t;

/* TODO: caching */
static fs_superblock_t* create_fs_superblock() {
  fs_superblock_t* ret = kmalloc(sizeof(fs_superblock_t));

  memset(ret, 0, sizeof(fs_superblock_t));

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

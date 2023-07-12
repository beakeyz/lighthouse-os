#include "superblock.h"

fs_superblock_t* create_fs_superblock() {
  fs_superblock_t* ret = kmalloc(sizeof(fs_superblock_t));

  memset(ret, 0, sizeof(fs_superblock_t));

  ret->m_ops_lock = create_mutex(0);

  return ret;
}

void destroy_fs_superblock(fs_superblock_t* block) {

  if (block->m_ops && block->m_ops->destroy_fs_specific_info && block->m_fs_specific_info)
    block->m_ops->destroy_fs_specific_info(block->m_fs_specific_info);

  if (block->m_ops_lock)
    destroy_mutex(block->m_ops_lock);

  kfree(block);
}

#ifndef __ANIVA_VFS_CACHE__
#define __ANIVA_VFS_CACHE__

#include "fs/vnode.h"

typedef struct {
  uintptr_t m_buffer;
  size_t m_length;
} read_cache_entry_t;

void init_vcache();

void cache_vnode(vnode_t);
bool is_vnode_cached(vnode_t*);

#endif // !__ANIVA_VFS_CACHE__

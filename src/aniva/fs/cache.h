#ifndef __ANIVA_VFS_CACHE__
#define __ANIVA_VFS_CACHE__

#include "fs/core.h"
#include "fs/vnode.h"

typedef struct {
  uintptr_t m_buffer;
  size_t m_length;
} read_cache_entry_t;

void init_vcache();

void cache_vnode(vnode_t);
bool is_vnode_cached(vnode_t*);

/*
 * Capabilities I want the caching system to have
 *  - every time we read a block, we should be able to place it into a list where we can easily find it 
 *    once we try to read that block again. With that comes that any writes to that block, should then 
 *    go to that cached block, in stead of to disk. This means we will have to sync with disk a lot in
 *    order to keep us from losing data...
 */

#endif // !__ANIVA_VFS_CACHE__

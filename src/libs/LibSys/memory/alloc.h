#ifndef __LIGHTENV_MEM_ALLOC__
#define __LIGHTENV_MEM_ALLOC__

/*
 * LibSys memory allocator
 * 
 * This allocator gets initialized right before the process starts execution in  
 * order to have a large memory pool ready to allocate in.
 * The process may:
 *  - Ask the allocator for a new large memory pool
 *  - Allocate in any existing pool
 *  - Invoke the deallocator from the allocator
 *  - Ask the allocator to allocate in another process (for that we use handles and 'allocator sharing')
 */

#include "LibSys/handle_def.h"
#include "LibSys/system.h"
#include "sys/types.h"

/* Minimum of 5 Kib in order to save pool integrity */
#define MIN_POOLSIZE            (0x1000)

enum {
  MEMPOOL_TYPE_DEFAULT      = 0,
  MEMPOOL_TYPE_BUFFER       = 1,
  MEMPOOL_TYPE_IO           = 2
};

/* I/O opperation permissions */
#define MEMPOOL_FLAG_R              (0x00000001)
#define MEMPOOL_FLAG_W              (0x00000002)
#define MEMPOOL_FLAG_RW             (MEM_POOL_FLAG_R | MEM_POOL_FLAG_W)

/* Proctect this pool from process-to-process memory allocation (allocator sharing) */
#define MEMPOOL_FLAG_PTPALLOC_PROT  (0x00000004)
/* Pool location resides in memory */
#define MEMPOOL_FLAG_MEMBACKED      (0x00000008)
/* Pool location resides on disk */
#define MEMPOOL_FLAG_DISKBACKED     (0x00000010)
/* This pool is used by the processes memory allocation stack */
#define MEMPOOL_FLAG_MALLOCPOOL     (0x00000020)

/*
 * Try to allocate a new pool for this process
 * The pool address is rounded up to the next (Most likely 4Kib) page 
 * aligned address. if pooladdr is NULL, 
 *
 * if the poolsize is less than MIN_POOLSIZE, the actual 
 * size gets rounded up to a multiple of MIN_POOLSIZE
 *
 * Flags indicate the protection level
 */
VOID* allocate_pool(
  __INOUT__             size_t* poolsize,
  __IN__                DWORD flags,
  __IN__ __OPTIONAL__   DWORD pooltype
);

DWORD deallocate_pool(
  __IN__                VOID* pooladdr,
  __IN__                size_t poolsize
);

/*
 * Advanced pool allocation
 * Allocates the pool in the address-space of another
 * process, specified by the handle
 * 
 * Otherwise, the same rules apply as to allocate_pool
 */
VOID* allocate_pool_av(
  __IN__                HANDLE handle,
  __INOUT__             size_t* poolsize,
  __IN__                DWORD flags,
  __IN__ __OPTIONAL__   DWORD pooltype
);

/*
 * Find out what flags this pool has
 */
int query_pool_flags(
  __IN__    VOID* pooladdr,
  __OUT__   DWORD* flags
);

/*
 * Customize the flags of a certain pool
 */
int modify_pool_flags(
  __IN__ VOID* pool,
  __IN__ DWORD flags
);

/*
 * Get the poolcount, either from this process, or another process
 * from which we have a handle
 */
int get_pool_count(
  __OUT__               DWORD* poolcount,
  __IN__ __OPTIONAL__   HANDLE handle
);

#endif // !__LIGHTENV_MEM_ALLOC__

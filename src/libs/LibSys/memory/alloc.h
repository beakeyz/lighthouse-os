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
 * The process may not:
 *  - Ask the allocator to allocate outside a pool
 *  - Ask the allocator to allocate in another process (for that we use handles and 'allocator sharing')
 */

#endif // !__LIGHTENV_MEM_ALLOC__

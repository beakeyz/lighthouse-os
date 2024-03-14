#include "alloc.h"
#include "lightos/memory/memflags.h"
#include "lightos/syscall.h"
#include "lightos/system.h"

#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

/*!
 * @brief: Simply asks the kernel for memory
 *
 * NOTE: These allocations might give nullptrs, but for this reason we can
 * check the @poolsize buffer to see if that is non-null
 */
VOID* allocate_pool(size_t* poolsize, DWORD flags, DWORD pooltype)
{
  VOID* buffer;
  QWORD result;
  size_t size;

  if (!poolsize || !(*poolsize))
    return NULL;

  buffer = NULL;
  size = ALIGN_UP(*poolsize, MEMPOOL_ALIGN);

  *poolsize = NULL;

  /* Request memory from the system */
  result = syscall_3(SYSID_ALLOC_PAGE_RANGE, size, flags, (uint64_t)&buffer);

  if (result != SYS_OK)
    return NULL;

  *poolsize = size;
  return buffer;
}

DWORD deallocate_pool(VOID* pooladdr, size_t poolsize)
{
  return NULL;
}

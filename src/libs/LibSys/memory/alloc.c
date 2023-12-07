#include "alloc.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"

#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

VOID* allocate_pool(size_t* poolsize, DWORD flags, DWORD pooltype)
{
  VOID* buffer;
  QWORD result;
  size_t size;

  if (!poolsize || !(*poolsize))
    return NULL;

  buffer = NULL;
  size = ALIGN_UP(*poolsize, 0x1000);

  /* Request memory from the system */
  result = syscall_3(SYSID_ALLOCATE_PAGES, size, NULL, (uint64_t)&buffer);

  if (result != SYS_OK || !buffer)
    return NULL;

  return buffer;
}

DWORD deallocate_pool(VOID* pooladdr, size_t poolsize)
{
  return NULL;
}

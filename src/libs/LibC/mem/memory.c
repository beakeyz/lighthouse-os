
#include "memory.h"
#include "LibSys/syscall.h"
#include <LibSys/system.h>

#define DEFAULT_INITIAL_RANGE_SIZE      (2 * Mib)

struct malloc_range {
  struct malloc_range* m_next;
  uint8_t m_bytes[];
};

static struct malloc_range* __start_range;

/*
 * Add a range of a pagealigned size
 */
static void __add_malloc_range(size_t size)
{
  void* buffer;
  syscall_result_t result;

  if (!size)
    return;

  result = syscall_3(SYSID_ALLOCATE_PAGES, size, NULL, (uint64_t)&buffer);

  if (result != SYS_OK)
    return;

  struct malloc_range* range = (struct malloc_range*)buffer;

  /* Point this range to the previous first range */
  range->m_next = __start_range;

  /* Smack this range right in front */
  __start_range = range;
}

/*
 * Until we have dynamic loading of libraries,
 * we want to keep libraries using libraries to a
 * minimum. This is because when we statically link 
 * everything together and for example we are using 
 * LibSys here, it gets included in the binary, which itself 
 * does not use any other functionallity of LibSys, so thats 
 * kinda lame imo
 *
 * TODO: check if the process has requested a heap of a specific size
 */
void init_memalloc()
{
  /* Initialize structures */
  /* Ask for a memory region from the kernel */
  /* Use the initial memory we get to initialize the allocator further */
  /* Mark readiness */

  __start_range = NULL;

  __add_malloc_range(DEFAULT_INITIAL_RANGE_SIZE);
}

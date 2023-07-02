
#include "memory.h"
#include "LibSys/syscall.h"
#include <LibSys/system.h>
#include <stdlib.h>

#define DEFAULT_INITIAL_RANGE_SIZE      (2 * Mib)

struct malloc_range {
  struct malloc_range* m_next;
  uint8_t m_bytes[];
};

static struct malloc_range* __start_range;

static size_t counter;

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

  exit((uint64_t)buffer);

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
void __init_memalloc(void)
{
  /* Initialize structures */
  /* Ask for a memory region from the kernel */
  /* Use the initial memory we get to initialize the allocator further */
  /* Mark readiness */

  __start_range = NULL;
  counter = 0;

  __add_malloc_range(DEFAULT_INITIAL_RANGE_SIZE);
}


void* mem_alloc(size_t size, uint32_t flags)
{
  if (!__start_range)
    return nullptr;

  /* EZ bump allocator */
  void* res = (void*)((uint8_t*)__start_range->m_bytes + counter);
  counter += size;
  return res;
}

void* mem_move_alloc(void* ptr, size_t new_size, uint32_t flags) 
{
  /* Also nope =) */
  return nullptr;
}

int mem_dealloc(void* addr, uint32_t flags)
{
  /* No */
  return 0;
}

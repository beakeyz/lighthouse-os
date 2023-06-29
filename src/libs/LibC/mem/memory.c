
#include "memory.h"

/*
 * Until we have dynamic loading of libraries,
 * we want to keep libraries using libraries to a
 * minimum. This is because when we statically link 
 * everything together and for example we are using 
 * LibSys here, it gets included in the binary, which itself 
 * does not use any other functionallity of LibSys, so thats 
 * kinda lame imo
 */
void init_memalloc()
{
  /* Initialize structures */
  /* Ask for a memory region from the kernel */
  /* Use the initial memory we get to initialize the allocator further */
  /* Mark readiness */
}

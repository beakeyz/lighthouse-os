#include "driver.h"
#include "mem/kmem.h"

/*
 * Driver memory allocation routines
 */

error_t driver_alloc_mem(driver_t* driver, void** presult, vaddr_t vbase, size_t bsize, u32 custom_flags, u32 page_flags)
{
    /* Allocate a kernel range on this drivers tracker */
    return kmem_alloc_range(presult, nullptr, &driver->resources.tracker, vbase, bsize, custom_flags, page_flags);
}

error_t driver_dealloc_mem(driver_t* driver, vaddr_t vaddr, size_t bsize)
{
    return kmem_dealloc(nullptr, &driver->resources.tracker, vaddr, bsize);
}

error_t driver_map_mem(driver_t* driver, void** presult, paddr_t pbase, vaddr_t vbase, size_t bsize, u32 custom_flags, u32 page_flags)
{
    return kmem_alloc_ex(presult, nullptr, &driver->resources.tracker, pbase, vbase, bsize, custom_flags, page_flags);
}


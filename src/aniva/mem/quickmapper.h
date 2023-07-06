#ifndef __ANIVA_QUICKMAPPER__
#define __ANIVA_QUICKMAPPER__

/*
 * This system ensures we can always access new memory, by having a reserved high virtual
 * memory address that we can map any physical address to
 */

#include "libk/flow/error.h"

/* There is a quickmapped page */
#define QUICKMAP_DIRTY (0x00000001)

void init_quickmapper();

ErrorOrPtr try_quickmap(paddr_t p);
ErrorOrPtr try_quickunmap();

/*
 * Fetch the current unaligned quickmapped address
 * (aka. The offset inside the physical page of the actual quickmapped address)
 */
vaddr_t get_quickmapped_addr();

vaddr_t quick_map(paddr_t p);
void quick_unmap();

ErrorOrPtr quick_map_ex(paddr_t p, uint32_t page_flags);

#endif // !__ANIVA_QUICKMAPPER__

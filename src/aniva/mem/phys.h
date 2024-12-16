#ifndef __ANIVA_MEM_PHYSICAL_H__
#define __ANIVA_MEM_PHYSICAL_H__

/*
 * This is the physical memory manager of kmem
 *
 * We need to keep track of references to different physical memory regions, so we don't give away memory
 * thats actively being used. This on it's own is a challenge, appart from regular kmem stuff, which is why
 * it's seperated from kmem
 */

#include "mem/kmem.h"
#include <lightos/types.h>

int init_kmem_phys(u64* mb_addr);
int init_kmem_phys_late();

size_t kmem_phys_get_total_bytecount();
kmem_range_t* kmem_phys_get_range(u32 idx);
u32 kmem_phys_get_nr_ranges();

/*
 * Physical page manipulation functions
 * TODO: Remove and replace with higher level and more robust functions
 * that also keep track of references and are locked correctly and crap
 */
bool kmem_phys_is_page_used(uintptr_t idx);

error_t kmem_phys_alloc_page(u64* p_page_idx);
error_t kmem_phys_dealloc_page(u64 page_idx);

error_t kmem_phys_reserve_range(u64 page_idx, u32 nr_pages);
error_t kmem_phys_alloc_range(u32 nr_pages, u64* p_page_idx);
error_t kmem_phys_dealloc_range(u64 page_idx, u32 nr_pages);

#endif // !__ANIVA_MEM_PHYSICAL_H__

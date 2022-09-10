#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__
#include <arch/x86/multiboot.h>

// defines for alignment
#define ALIGN_UP(addr, size) \
    ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

void init_kmem_manager (struct multiboot_tag_mmap* mmap, uintptr_t mb_first_addr);

#endif // !__KMEM_MANAGER__

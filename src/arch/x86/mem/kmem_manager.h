#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>

typedef struct {
    uint32_t mmap_entry_num;
    multiboot_memory_map_t* mmap_entries;
} kmem_data_t;

// defines for alignment
#define ALIGN_UP(addr, size) \
    ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

void init_kmem_manager (uint32_t mb_addr, uint32_t mb_first_addr, struct multiboot_tag_basic_meminfo* basic_info);
void prep_mmap (struct multiboot_tag_mmap* mmap);
void* get_bitmap_region (uint64_t limit, size_t bytes);

#endif // !__KMEM_MANAGER__

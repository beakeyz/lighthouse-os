#include "kmem_manager.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include "arch/x86/multiboot.h"
#include <libc/stddef.h>
#include <libc/string.h>

static kmem_data_t kmem_data;
static kmem_bitmap_t kmem_bitmap;

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
    kmem_data.mmap_entry_num = (mmap->size - sizeof(*mmap))/mmap->entry_size;
    kmem_data.mmap_entries = (multiboot_memory_map_t*)mmap->entries;
}

void init_kmem_manager(uint32_t mb_addr, uint32_t mb_size, struct multiboot_tag_basic_meminfo* basic_info) {
    init_bitmap(&kmem_bitmap, basic_info, mb_addr + mb_size); 
    uint64_t bitmap_start_addr;
    size_t bitmap_size;
    bm_get_region(&kmem_bitmap, &bitmap_start_addr, &bitmap_size); 

    
    println("yay, reached test end");
}

// then init, after kmem_manager is initialized
void init_mmap () {

}

void* get_bitmap_region (uint64_t limit, uint64_t bytes) {

    for (uintptr_t i = 0; i < kmem_data.mmap_entry_num; i++) {
        multiboot_memory_map_t* map = &kmem_data.mmap_entries[i];

        // If we're able to use this region
        if (map->type == MULTIBOOT_MEMORY_AVAILABLE) {
            // If the region lies in the higher half
            if (map->addr + map->len > limit) {
                size_t offest = limit > map->addr ? limit - map->addr : 0;
                size_t available_space = map->len - offest;

                if (available_space > bytes) {
                    println("found a region =D");
                    return (void*)(map->addr + offest);
                }
            }
        }
    }
    println("no region found, thats a yikes =/");
    return NULL;
}


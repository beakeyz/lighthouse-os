#include "multiboot.h"
#include "kernel/dev/debug/serial.h"
#include "libk/string.h"
#include <libk/stddef.h>

void mb_initialize(void *addr, uintptr_t* highest_addr, uintptr_t* first_valid_alloc_addr) {
    uintptr_t offset = 0;

    // first: find the memorymap 
    struct multiboot_tag_mmap* mb_memmap = get_mb2_tag(addr, 6);
    if (!mb_memmap) {
        println("No memorymap found! aborted");
        return;
    }
    void* entry = mb_memmap->entries;

    while ((uintptr_t)entry < (uintptr_t)mb_memmap + mb_memmap->size) {
        struct multiboot_mmap_entry* cur_entry = entry;
        uintptr_t cur_offset = cur_entry->addr + cur_entry->len - 1; 
        if (cur_entry->type == 1 && cur_entry->len && cur_offset > offset) {
            offset = cur_offset;
        }
        entry += mb_memmap->entry_size;
    }

    // TODO: second: modules
    struct multiboot_tag_module* mods = get_mb2_tag((void*)addr, 3); 
    while (mods) {
        uintptr_t a = (uintptr_t)mods->mod_end;
        if (a > *first_valid_alloc_addr) *first_valid_alloc_addr = a;
        mods = next_mb2_tag((char*)mods + mods->size, 3);
    }
    
    *highest_addr = offset;
    *first_valid_alloc_addr = (*first_valid_alloc_addr + 0xFFF) & 0xFFFFffffFFFFf000;
}

void* next_mb2_tag(void *cur, uint32_t type) {
    void* idxPtr = cur;
    struct multiboot_tag* tag = idxPtr;
    // loop through the tags to find the right type
    while (true) {
        if (tag->type == type) return tag;
        if (tag->type == 0) return nullptr;

        idxPtr += tag->size;
        while ((uintptr_t)idxPtr & 7) idxPtr++;
        tag = idxPtr;
    }
} 

void* get_mb2_tag(void *addr, uint32_t type) {
    char* header = ((void*) addr) + 8;
    return next_mb2_tag(header, type);
}

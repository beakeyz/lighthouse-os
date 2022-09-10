#include "arch/x86/dev/debug/serial.h"
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>

uintptr_t mb_initialize(void *addr) {
    static uintptr_t offset = 0;

    // first: find the memorymap 
    struct multiboot_tag_mmap* mb_memmap = get_mb2_tag(addr, 6);
    if (!mb_memmap) {
        println("No memorymap found! aborted");
        return -1;
    }
    void* entry = mb_memmap->entries;
    while ((uintptr_t)entry < (uintptr_t)mb_memmap + mb_memmap->size) {
        struct multiboot_mmap_entry* cur_entry = entry;
        uintptr_t cur_offset = cur_entry->addr + cur_entry->len - 1; 
        if (cur_entry->type == 1 && cur_entry->len && cur_offset > offset) {
            offset = cur_offset;
            println("shifted offset");
        }
        entry += mb_memmap->entry_size;
        println("shifted entry");
    }

    // TODO: second: modules

    offset = (offset + 0x1FFFFF) & 0xFFFfffFFFf000;
    return offset;
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

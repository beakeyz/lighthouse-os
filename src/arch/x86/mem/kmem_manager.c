#include "kmem_manager.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include "arch/x86/multiboot.h"
#include <libc/stddef.h>
#include <libc/string.h>

static kmem_bitmap_t kmem_bitmap;
static uintptr_t kmem_bitmap_addr;
static uintptr_t kmem_as_size;
static uintptr_t kmem_available_mem;

static uint64_t get_free_RAM_length (struct multiboot_tag_mmap* mmap) {
    // find the base and length of the RAM
    const uint64_t last_idx = mmap->size - 1;
    uint64_t addr = mmap->entries[last_idx].addr;
    uint64_t len = mmap->entries[last_idx].len;
    // align it to our pages
    uint64_t aligned_addr = addr - (addr % PAGE_SIZE); 
    // this one is a lil weird
    uint64_t aligned_len = len + PAGE_SIZE - (len % PAGE_SIZE);
    // compute the length of our free range
    return aligned_addr + aligned_len - 1;
} 

/*
static uint64_t get_free_RAM_addr (struct multiboot_tag_mmap* mmap) {
     // find the base of the RAM
    uint64_t last_idx = mmap->size - 1;
    uint64_t addr = mmap->entries[last_idx].addr;
    // align it to our pages
    uint64_t aligned_addr = addr - (addr % PAGE_SIZE); 

    return aligned_addr;
}
*/

void init_kmem_manager(struct multiboot_tag_mmap* mmap, uintptr_t mb_first_addr) {
    
    // Small outline of our pmm
    // 1: find out what memory we can use and what is off limits (use bl data for this)
    // 2: place the kernel heap (cuz lets do that too) somewhere
    // 3: 

    println("getting ram length!");
    size_t mem_length = get_free_RAM_length(mmap);
    size_t aligned_mem_length = mem_length / PAGE_SIZE;
    kmem_as_size = 0;
    kmem_bitmap_addr = 0;

    // loop 0
    for (uintptr_t i = 0; i < mmap->size; i++) {
        struct multiboot_mmap_entry* entry = &mmap->entries[i];
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            // yes =D
            if (entry->len > ((aligned_mem_length / 8) + (PAGE_SIZE * 2))) {
                println("slayyy");
                kmem_bitmap_addr = entry->addr + PAGE_SIZE * 2;
                kmem_as_size = aligned_mem_length;
                break;
            }
            println("slay test");
        } 
    } 

    kmem_as_size = 0xFFFFF;

    // init bitmap

    kmem_bitmap.bm_buffer = (uint8_t*) kmem_bitmap_addr;
    kmem_bitmap.bm_size = kmem_as_size;
    kmem_bitmap.bm_last_allocated_bit = 0;

    memset((void*)kmem_bitmap_addr, 0xff, kmem_as_size / 8);

    for (uintptr_t i = 0; i < mmap->size; i++) {
        struct multiboot_mmap_entry* entry = &mmap->entries[i];
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            println("Marking range as free!");
            size_t start = ((entry->addr) + PAGE_SIZE - ((entry->addr) % PAGE_SIZE)) / PAGE_SIZE; // align up
            size_t size = ((entry->len) - ((entry->len) % PAGE_SIZE)) / PAGE_SIZE;          // align down
            bm_mark_block_free(&kmem_bitmap, start, size - 1);
            kmem_available_mem += size;
        }
    }

    bm_mark_block_used(&kmem_bitmap, (kmem_bitmap_addr / PAGE_SIZE), (kmem_as_size / 8) / PAGE_SIZE);
    kmem_bitmap.bm_last_allocated_bit = 0;
}


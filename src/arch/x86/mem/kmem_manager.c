#include "kmem_manager.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmalloc.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include "arch/x86/mem/pml.h"
#include "arch/x86/multiboot.h"
#include "libc/linkedlist.h"
#include <libc/stddef.h>
#include <libc/string.h>


static kmem_data_t kmem_data;

/**
 * bitmap page allocator for 4KiB pages
 */
//static volatile uint32_t *frames;
static size_t nframes;
//static size_t total_memory = 0;
//static size_t unavailable_memory = 0;
//static uint8_t * mem_refcounts = NULL;

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
    kmem_data.mmap_entry_num = (mmap->size - sizeof(*mmap))/mmap->entry_size;
    kmem_data.mmap_entries = (multiboot_memory_map_t*)mmap->entries;
}

// this layout is inspired by taoruos
#define __def_pagemap __attribute__((aligned(0x1000UL))) = {0}
#define standard_pd_entries 512
pml_t init_page_maps[3][standard_pd_entries] __def_pagemap;
pml_t high_base_pml[standard_pd_entries] __def_pagemap;
pml_t heap_base_pml[standard_pd_entries] __def_pagemap;
pml_t heap_base_pd[standard_pd_entries] __def_pagemap;
pml_t heap_base_pt[3*standard_pd_entries] __def_pagemap;
pml_t low_base_pmls[34][standard_pd_entries] __def_pagemap;
pml_t twom_high_pds[64][standard_pd_entries] __def_pagemap;

// TODO: page directory and range abstraction and stuff lol
void init_kmem_manager(uint32_t mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {
    struct multiboot_tag_mmap* mmap = get_mb2_tag((void*)mb_addr, 6); 
    prep_mmap(mmap);

    println("setting stuff");
    asm volatile (
        "movq %%cr0, %%rax\n"
        "orq $0x10000, %%rax\n"
        "movq %%rax, %%cr0\n"
        : : : "rax");


    init_page_maps[0][511].raw_bits = (uint64_t)&high_base_pml | 0x03;
    init_page_maps[0][510].raw_bits = (uint64_t)&heap_base_pml | 0x03;

    for (uintptr_t i = 0; i < 64; ++i) {
        high_base_pml[i].raw_bits = (uint64_t)&twom_high_pds[i] | 0x03;
        for (uintptr_t j = 0; j < standard_pd_entries; ++j) {
            twom_high_pds[i][j].raw_bits = ((i << 30) + (j << 21)) | 0x80 | 0x03;
        }
    }

    low_base_pmls[0][0].raw_bits = (uint64_t)&low_base_pmls[1] | 0x07;

    uintptr_t end_ptr = ((uintptr_t)&_kernel_end + PAGE_LOW_MASK) & PAGE_SIZE_MASK;
    size_t num_low_pages = end_ptr >> 12;
    size_t pd_count = (size_t)((num_low_pages + ENTRY_MASK) >> 9);

    for (int i = 0; i < pd_count; ++i) {
        low_base_pmls[1][i].raw_bits = (uint64_t)&low_base_pmls[2+i] | 0x03;
        for (int j = 0; j < standard_pd_entries; ++j) {
            low_base_pmls[2+i][j].raw_bits = (uint64_t)(0x200000UL * i + 0x1000UL * j) | 0x03;
        }
    }

    low_base_pmls[2][0].raw_bits = 0;

    init_page_maps[0][0].raw_bits = (uint64_t)&low_base_pmls[0] | 0x07;

    nframes = (first_valid_addr >> 12);
    size_t frames_bytes = (INDEX_FROM_BIT(nframes * 8) + PAGE_LOW_MASK) & PAGE_SIZE_MASK;
    size_t frames_pages = frames_bytes >> 12;
    first_valid_alloc_addr = (first_valid_alloc_addr + PAGE_LOW_MASK) & PAGE_SIZE_MASK;

    heap_base_pml[0].raw_bits = (uint64_t)&heap_base_pd | 0x03;
    heap_base_pd[0].raw_bits = (uint64_t)&heap_base_pt[0 * 512] | 0x03;
    heap_base_pd[1].raw_bits = (uint64_t)&heap_base_pt[1 * 512] | 0x03;
    heap_base_pd[2].raw_bits = (uint64_t)&heap_base_pt[2 * 512] | 0x03;

    if (frames_pages > 3 * standard_pd_entries) {
        println("warning: frames_pages > 3*512");
    }

    for (int i = 0; i < frames_pages; ++i) {
        // shift i back by 12 to get original byte
        heap_base_pt[i].raw_bits = (first_valid_alloc_addr + (i << 12)) | 0x03;
    }
    uintptr_t map = (uintptr_t)kmem_from_phys((uintptr_t)((pml_t*)&init_page_maps[0])) & 0x7fffffffffUL;

    asm volatile ("" : : : "memory");
    asm volatile ("movq %0, %%cr3" :: "r"(map));
    asm volatile ("" : : : "memory");
    
    println("loaded the new pagemaps!");
    
    for (;;) {}

    parse_memmap();

}

void parse_memmap () {

    // NOTE: negative one stands for the kernel range
    phys_mem_range_t kernel_range = { -1, _kernel_start, _kernel_end - _kernel_start };
    println(to_string(_kernel_start));

    for (uintptr_t i = 0; i < kmem_data.mmap_entry_num; i++) {
        multiboot_memory_map_t* map = &kmem_data.mmap_entries[i];

        uint64_t addr = map->addr;
        uint64_t length = map->len;

        // FIXME: with '- VIRTUAL_BASE' it does not crash, so this means that something is going wrong
        // while setting up paging =/
        //phys_mem_range_t range = { map->type, addr - VIRTUAL_BASE, length };
    
        println(to_string(addr));
        println(to_string(length));

        if (map->type != MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }

        uintptr_t diff = addr % PAGE_SIZE;
        if (diff != 0) {
            println("missaligned region!");
            diff = PAGE_SIZE - diff;
            addr += diff;
            length -= diff;
        }
        if ((length % PAGE_SIZE) != 0) {
            println("missaligned length!");
            length -= length % PAGE_SIZE;
        }
        if (length < PAGE_SIZE) {
            println("page is too small!");
        }

        
        for (uint64_t page_base = addr;  page_base <= (addr + length); page_base += PAGE_SIZE) {
            
            //println("yeet");
            if (page_base >= kernel_range.start && page_base <= kernel_range.start + kernel_range.length) {
                println("page base was found inside a used range");
                continue;
            }

        }
    }
    println("no region found, thats a yikes =/");
}

void* kmem_from_phys(uintptr_t addr) {
    return (void*)(addr | HIGH_MAP_BASE);
}

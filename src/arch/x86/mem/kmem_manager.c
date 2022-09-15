#include "kmem_manager.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmalloc.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include "arch/x86/multiboot.h"
#include "libc/linkedlist.h"
#include <libc/stddef.h>
#include <libc/string.h>

// 2MB initial heap
//__attribute__((section(".heap"))) static uint8_t _initial_heap_mem[2*1024];

static kmem_data_t kmem_data;
static kmem_bitmap_t kmem_bitmap;

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
    kmem_data.mmap_entry_num = (mmap->size - sizeof(*mmap))/mmap->entry_size;
    kmem_data.mmap_entries = (multiboot_memory_map_t*)mmap->entries;
}

// TODO: page directory and range abstraction and stuff lol
void init_kmem_manager(uint32_t mb_addr, uint32_t mb_size, struct multiboot_tag_basic_meminfo* basic_info) {

    println(to_string(basic_info->mem_lower));
    println(to_string(basic_info->mem_upper));
    println("--------");
    
    parse_memmap();

}

// then init, after kmem_manager is initialized
// (this is basically just marking our memmap as non-writable xD)
void init_mmap (struct multiboot_tag_basic_meminfo* basic_info) {
    kmem_data.reserved_phys_count = 0;
    if (kmem_bitmap.bm_used_frames > 0) {
        uint32_t counter = 0;
        uint64_t mem_limit = (basic_info->mem_upper + 1024) * 1024;
        while(counter < kmem_data.mmap_entry_num){
            if(kmem_data.mmap_entries[counter].addr < mem_limit &&
                    kmem_data.mmap_entries[counter].type > 1){
               bm_mark_block_used(&kmem_bitmap, kmem_data.mmap_entries[counter].addr, kmem_data.mmap_entries[counter].len);
               kmem_data.reserved_phys_count++;
            }
            counter++;
        }
    }
}

void parse_memmap () {

    // NOTE: negative one stands for the kernel range
    phys_mem_range_t kernel_range = { -1, _kernel_start, VIRTUAL_BASE };
    println(to_string(_kernel_start));

    for (uintptr_t i = 0; i < kmem_data.mmap_entry_num; i++) {
        multiboot_memory_map_t* map = &kmem_data.mmap_entries[i];

        uint64_t addr = map->addr;
        uint64_t length = map->len;

        // FIXME: with '- VIRTUAL_BASE' it does not crash, so this means that something is going wrong
        // while setting up paging =/
        //phys_mem_range_t range = { map->type, addr - VIRTUAL_BASE, length };
    
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

        println(to_string(addr));
        println(to_string(length));
        
        for (uint64_t page_base = addr - VIRTUAL_BASE;  page_base <= (addr - VIRTUAL_BASE + length); page_base += PAGE_SIZE) {
            
            //println("yeet");
            if (page_base >= kernel_range.start && page_base <= kernel_range.start + kernel_range.length) {
                println("page base was found inside a used range");
                continue;
            }

        }
    }
    println("no region found, thats a yikes =/");
}

void* alloc_frame () {

    size_t frame = bm_allocate(&kmem_bitmap, 1);
    if (frame > 0) {
        kmem_bitmap.bm_used_frames++;
        return (void*)(frame * PAGE_SIZE_BYTES);
    }

    return nullptr;
} 

void* phys_to_virt(void *phys, void *virt, int flags) {
    uint16_t pml4_e = PML4_ENTRY((uint64_t) virt);
    uint64_t *pml4_table = (uint64_t *) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l,510l,510l,510l));
    
    uint16_t pdpr_e = PDPR_ENTRY((uint64_t) virt);
    uint64_t *pdpr_table = (uint64_t *) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l,510l,510l, (uint64_t) pml4_e));

    uint64_t *pd_table = (uint64_t *) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l,510l, (uint64_t) pml4_e, (uint64_t) pdpr_e));
    uint16_t pd_e = PD_ENTRY((uint64_t) virt);

    if (!(pml4_table[pml4_e] & 0b1)) {
        println("new pml4 thing lol");
        uintptr_t* new_table = alloc_frame();
        pml4_table[pml4_e] = (uint64_t) new_table | 0 | WRITE_BIT | PRESENT_BIT;
        clear_table(pdpr_table);
    }

    if( !(pdpr_table[pdpr_e] & 0b1) ) {
        println("new pdpr thing lol");
        uintptr_t *new_table = alloc_frame();
        pdpr_table[pdpr_e] = (uint64_t) new_table | 0 | WRITE_BIT | PRESENT_BIT;
        clear_table(pd_table);
    }
     
    if( !(pd_table[pd_e] & 0b01) ) {
        println("new pd thing lol");
        pd_table[pd_e] = (uint64_t) (phys) | WRITE_BIT | PRESENT_BIT | HUGEPAGE_BIT | flags;
    }

    return virt;
}

void* get_vaddr(void *vaddr, int flags) {
    void* new_addr = alloc_frame();
    return phys_to_virt(new_addr, vaddr, flags);
}

void clear_table (uintptr_t* table) {
    for (int i = 0; i < VM_PAGES_PER_TABLE; i++) {
        table[i] = 0x00l;
    }
}

uint64_t ensure_address_in_higher_half( uint64_t address ) {
    if ( address > HIGHER_HALF_ADDRESS_OFFSET ) {
        return address;
    }
    return address + HIGHER_HALF_ADDRESS_OFFSET;
}

bool is_address_higher_half(uint64_t address) {
    if ( address & (1l << 62) ) {
        return true;
    }
    return false;
}

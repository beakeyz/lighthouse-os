#include "kmem_manager.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmalloc.h"
#include "arch/x86/mem/pml.h"
#include "arch/x86/multiboot.h"
#include "libc/linkedlist.h"
#include <libc/stddef.h>
#include <libc/string.h>


static kmem_data_t kmem_data;

/*
 *  bitmap page allocator for 4KiB pages
 */
static volatile uint32_t *frames;
static size_t nframes;
static size_t total_memory = 0;
static size_t unavailable_memory = 0;
static uintptr_t lowest_available = 0;

/*
*   heap crap
*/
static char* heap_start = NULL;


// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
    kmem_data.mmap_entry_num = (mmap->size - sizeof(*mmap))/mmap->entry_size;
    kmem_data.mmap_entries = (struct multiboot_mmap_entry*)mmap->entries;
}

// this layout is inspired by taoruos
#define __def_pagemap __attribute__((aligned(0x1000UL))) = {0}
#define standard_pd_entries 512
pml_t base_init_pml[3][standard_pd_entries] __def_pagemap;
pml_t high_base_pml[standard_pd_entries] __def_pagemap;
pml_t heap_base_pml[standard_pd_entries] __def_pagemap;
pml_t heap_base_pd[standard_pd_entries] __def_pagemap;
pml_t heap_base_pt[3*standard_pd_entries] __def_pagemap;
pml_t low_base_pmls[34][standard_pd_entries] __def_pagemap;
pml_t twom_high_pds[64][512] __def_pagemap;

// TODO: page directory and range abstraction and stuff lol
void init_kmem_manager(uint32_t mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {
    struct multiboot_tag_mmap* mmap = get_mb2_tag((void*)mb_addr, 6); 
    prep_mmap(mmap);

    // 4294967295
    println("setting stuff");
    asm volatile (
        "movq %%cr0, %%rax\n"
        "orq $0x10000, %%rax\n"
        "movq %%rax, %%cr0\n"
        : : : "rax");


    base_init_pml[0][511].raw_bits = (uint64_t)&high_base_pml | 0x03;
    base_init_pml[0][510].raw_bits = (uint64_t)&heap_base_pml | 0x03;

    for (size_t i = 0; i < 64; ++i) {
        high_base_pml[i].raw_bits = (uint64_t)&twom_high_pds[i] | 0x03;
        for (size_t j = 0; j < 512; j++) {
            twom_high_pds[i][j].raw_bits = ((i << 30) + (j << 21)) | 0x80 | 0x03;
        }
    }

    low_base_pmls[0][0].raw_bits = (uint64_t)&low_base_pmls[1] | 0x07;

    uintptr_t end_ptr = ((uintptr_t)&_kernel_end + PAGE_LOW_MASK) & PAGE_SIZE_MASK;
    size_t num_low_pages = end_ptr >> 12;
    size_t pd_count = (size_t)((num_low_pages + ENTRY_MASK) >> 9);

    for (size_t i = 0; i < pd_count; ++i) {
        low_base_pmls[1][i].raw_bits = (uint64_t)&low_base_pmls[2+i] | 0x03;
        for (size_t j = 0; j < 512; ++j) {
            low_base_pmls[2+i][j].raw_bits = (uint64_t)(PAGE_SIZE_BYTES * i + SMALL_PAGE_SIZE * j) | 0x03;
        }
    }

    low_base_pmls[2][0].raw_bits = 0;

    base_init_pml[0][0].raw_bits = (uint64_t)&low_base_pmls[0] | 0x07;

    nframes = (first_valid_addr >> 12);
    size_t frames_bytes = (INDEX_FROM_BIT(nframes * 8) + PAGE_LOW_MASK) & PAGE_SIZE_MASK;
    size_t frames_pages = frames_bytes >> 12;
    first_valid_alloc_addr = (first_valid_alloc_addr + PAGE_LOW_MASK) & PAGE_SIZE_MASK;

    heap_base_pml[0].raw_bits = (uint64_t)&heap_base_pd | 0x03;
    heap_base_pd[0].raw_bits = (uint64_t)&heap_base_pt[0] | 0x03;
    heap_base_pd[1].raw_bits = (uint64_t)&heap_base_pt[512] | 0x03;
    heap_base_pd[2].raw_bits = (uint64_t)&heap_base_pt[1024] | 0x03;

    if (frames_pages > 3 * 512) {
        println("warning: frames_pages > 3*512");
    }

    for (size_t i = 0; i < frames_pages; ++i) {
        // shift i back by 12 to get original byte
        heap_base_pt[i].raw_bits = (first_valid_alloc_addr + (i << 12)) | 0x03;
    }

    uintptr_t map = (uintptr_t)kmem_from_phys((uintptr_t)((pml_t*)&base_init_pml[0])) & 0x7FFFffffFFUL;

    asm volatile ("movq %0, %%cr3" :: "r"(map));
    
    println("loaded the new pagemaps!");
    
    // FIXME: this statement is prob not necessary anymore, since we
    // parse the mmap prior to the kmem init, where we pass the important values
    // but I'll keep it here (commented) for now, since it does not harm anyone
    //parse_memmap();

    frames = (void*)((uintptr_t)KRNL_HEAP_START);
    // FIXME: I would like to fuck up the heap, but there is a lil issue that's kinda preventing me from doing so, 
    // namely just the fact that this little memset kinda corrupts my whole memoryspace -_-
    //memset((void*)frames, 0xFF, frames_bytes);
    
    for (uintptr_t i = 0; i < kmem_data.mmap_entry_num; i++) {
        multiboot_memory_map_t entry = kmem_data.mmap_entries[i];
        if (entry.type == 1) {

            for (uintptr_t base = entry.addr; base < entry.addr + (entry.len & 0xFFFFffffFFFFf000); base += 0x1000) {
                kmem_mark_frame_free(base);
            }
        }
    }

    // lets walk the heap (yay I love loops sooo much)
    size_t yes = 0;
    size_t no = 0;
    // go by each frame
    for (size_t i = 0; i < INDEX_FROM_BIT(nframes); i++) {
        for (size_t j = 0; j < 32; j++) {
            if (frames[i] & (uint32_t)(0x1 << j)) {
                no++;
            } else {
                yes++;
            }
        }
    }
    total_memory = yes*4;
    unavailable_memory = no*4;

    // let's quickly dbprint these memory stats
    print("Total available memory: "); println(to_string(total_memory));
    print("Total unavailable memory: "); println(to_string(unavailable_memory));

    // mark everything up to the first_valid_alloc_addr as used (cuz, uhm, it is)
    for (uintptr_t i = 0; i < first_valid_alloc_addr + frames_bytes; i += SMALL_PAGE_SIZE) {
        kmem_mark_frame_used(i);
    }

    heap_start = (char*)(KRNL_HEAP_START + frames_bytes);

    // TODO: refcounts
    println("done");

}

void parse_memmap () {

    // NOTE: negative one stands for the kernel range
    phys_mem_range_t kernel_range = { -1, (uint64_t)&_kernel_start, (uint64_t)(&_kernel_end - &_kernel_start) };
    println(to_string((uint64_t)&_kernel_start));

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

uintptr_t kmem_to_phys(pml_t *root, uintptr_t addr) {
    if (!root) root = base_init_pml[0];

    uintptr_t page_addr = (addr & 0xFFFFffffFFFFUL) >> 12;
    uint_t pml4_e = (page_addr >> 27) & ENTRY_MASK;
    uint_t pdp_e = (page_addr >> 18) & ENTRY_MASK;
    uint_t pd_e = (page_addr >> 9) & ENTRY_MASK;
    uint_t pt_e = (page_addr) & ENTRY_MASK;

    if (!root[pml4_e].structured_bits.present_bit)
        return -1;

    pml_t* pdp = kmem_from_phys((uintptr_t)root[pml4_e].structured_bits.page << 12);

    if (!pdp[pdp_e].structured_bits.present_bit) return (uintptr_t)-2;
	if (pdp[pdp_e].structured_bits.size) return ((uintptr_t)pdp[pdp_e].structured_bits.page << 12) | (addr & PDP_MASK);

	pml_t* pd = kmem_from_phys((uintptr_t)pdp[pdp_e].structured_bits.page << 12);

	if (!pd[pd_e].structured_bits.present_bit) return (uintptr_t)-3;
	if (pd[pd_e].structured_bits.size) return ((uintptr_t)pd[pd_e].structured_bits.page << 12) | (addr & PD_MASK);

	pml_t* pt = kmem_from_phys((uintptr_t)pd[pd_e].structured_bits.page << 12);

	if (!pt[pt_e].structured_bits.present_bit) return (uintptr_t)-4;
	return ((uintptr_t)pt[pt_e].structured_bits.page << 12) | (addr & PT_MASK);
}

void kmem_mark_frame_used (uintptr_t frame)
{
    if (frame < nframes * SMALL_PAGE_SIZE) {
        uintptr_t real_frame = frame >> 12;
        uintptr_t idx = INDEX_FROM_BIT(real_frame);
        uint32_t bit = OFFSET_FROM_BIT(real_frame);
        frames[idx] |= ((uint32_t)1 << bit);
        asm ("" ::: "memory");
    }
}

void kmem_mark_frame_free (uintptr_t frame)
{
    if (frame < nframes * SMALL_PAGE_SIZE) {
        uintptr_t real_frame = frame >> 12;
        uintptr_t idx = INDEX_FROM_BIT(real_frame);
        uint32_t bit = OFFSET_FROM_BIT(real_frame);

        frames[idx]  &= ~((uint32_t)1 << bit);
		asm ("" ::: "memory");
		if (frame < lowest_available) lowest_available = frame;

    }
}

// combination of the above two
void kmem_mark_frame(uintptr_t frame, bool value) {
    if (frame < nframes * SMALL_PAGE_SIZE) {
        uintptr_t real_frame = frame >> 12;
        uintptr_t idx = INDEX_FROM_BIT(real_frame);
        uint32_t bit = OFFSET_FROM_BIT(real_frame);

        if (value) {
            frames[idx] |= ((uint32_t)1 << bit);
		    asm ("" ::: "memory");
            return;
        }

        frames[idx] &= ~((uint32_t)1 << bit);
		asm ("" ::: "memory");
		if (frame < lowest_available) lowest_available = frame;
    }
}

uintptr_t kmem_get_frame () {
    for (uintptr_t i = INDEX_FROM_BIT(lowest_available); i < INDEX_FROM_BIT(nframes); i++) {
        if (frames[i] != (uint32_t)-1) {
            for (uintptr_t j = 0; j < (sizeof(uint32_t)*8); ++j) {
				if (!(frames[i] & 1 << j)) {
					uintptr_t out = (i << 5) + j;
					lowest_available = out + 1;
					return out;
				}
			}
        }
    } 
    return NULL;
}

pml_t* kmem_get_krnl_dir () {
    return kmem_from_phys((uintptr_t)&base_init_pml[0]);
}

pml_t* kmem_get_page (uintptr_t addr, unsigned int flags) {
    uintptr_t page_addr = (addr & 0xFFFFffffFFFFUL) >> 12;
    uint_t pml4_e = (page_addr >> 27) & ENTRY_MASK;
    uint_t pdp_e = (page_addr >> 18) & ENTRY_MASK;
    uint_t pd_e = (page_addr >> 9) & ENTRY_MASK;
    uint_t pt_e = (page_addr) & ENTRY_MASK;

    if (!base_init_pml[0][pml4_e].structured_bits.present_bit) {
        println("new pml4");
        if (flags & KMEM_GET_MAKE) {
            uintptr_t new = kmem_get_frame() << 12;
            kmem_mark_frame_used(new);
            memset(kmem_from_phys(new), 0, SMALL_PAGE_SIZE);
            base_init_pml[0][pml4_e].raw_bits = new | 0x07;
        } else {
            return nullptr;
        }
    }

    pml_t* pdp = kmem_from_phys(base_init_pml[0][pml4_e].structured_bits.page << 12);
    
    if (!pdp[pdp_e].structured_bits.present_bit) {
        println("new pdp");
        if (flags & KMEM_GET_MAKE) {
            uintptr_t new = kmem_get_frame() << 12;
            kmem_mark_frame_used(new);
            memset(kmem_from_phys(new), 0, SMALL_PAGE_SIZE);
            pdp[pdp_e].raw_bits = new | 0x07;
        } else {
            return nullptr;
        }
    }

    if (pdp[pdp_e].structured_bits.size) {
        println("Tried to get big page!");
        return nullptr;
    }

    pml_t* pd = kmem_from_phys(pdp[pdp_e].structured_bits.page << 12);
    
    if (!pd[pd_e].structured_bits.present_bit) {
        println("new pd");
        if (flags & KMEM_GET_MAKE) {
            uintptr_t new = kmem_get_frame() << 12;
            kmem_mark_frame_used(new);
            memset(kmem_from_phys(new), 0, SMALL_PAGE_SIZE);
            pd[pd_e].raw_bits = new | 0x07;
        } else {
            return nullptr;
        }
    }

    if (pd[pd_e].structured_bits.size) {
        println("tried to get 2MB page!");
        return nullptr;
    }

    pml_t* pt = kmem_from_phys(pd[pd_e].structured_bits.page << 12);
    
    if (pt->structured_bits.page != 0) {
        println("jummy memory");
    }
    return (pml_t*)&pt[pt_e];
}

void kmem_set_page_flags (pml_t* page, unsigned int flags) {
    if (page->structured_bits.page == 0) {
        uintptr_t frame = kmem_get_frame();
        kmem_mark_frame_used(frame << 12);
        page->structured_bits.page = frame;
    }

    page->structured_bits.size          = 0;
    page->structured_bits.present_bit   = 1;
    page->structured_bits.writable_bit  = (flags & KMEM_FLAG_WRITABLE) ? 1 : 0;
    page->structured_bits.user_bit      = (flags & KMEM_FLAG_KERNEL) ? 0 : 1;
    page->structured_bits.nocache_bit   = (flags & KMEM_FLAG_NOCACHE) ? 1 : 0;
    page->structured_bits.writethrough_bit = (flags & KMEM_FLAG_WRITETHROUGH) ? 1 : 0;
    page->structured_bits.size          = (flags & KMEM_FLAG_SPEC) ? 1 : 0;
    page->structured_bits.nx            = (flags & KMEM_FLAG_NOEXECUTE) ? 1 : 0;
}

void* kmem_alloc(size_t size) {
    if (!heap_start) {
        return nullptr;
    }

    if (size & PAGE_LOW_MASK) {
        println("invalid size!");
        return nullptr;
    }

    // TODO: locking
    void* start = heap_start;

    for (uintptr_t i = (uintptr_t)start; i < (uintptr_t)(start + size); i += SMALL_PAGE_SIZE) {
        pml_t* page = kmem_get_page(i, KMEM_GET_MAKE);
        kmem_set_page_flags(page, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);
    }

    heap_start += size;
    return start;
}

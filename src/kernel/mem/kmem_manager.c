#include "kmem_manager.h"
#include "kernel/dev/debug/serial.h"
#include "kernel/kmain.h"
#include "kernel/mem/kmalloc.h"
#include "kernel/mem/pml.h"
#include "kernel/libk/multiboot.h"
#include "libk/linkedlist.h"
#include <libk/stddef.h>
#include <libk/string.h>

typedef struct {
  uint32_t m_mmap_entry_num;
  multiboot_memory_map_t* m_mmap_entries;
  uint8_t m_reserved_phys_count;

  volatile uint8_t* m_physical_pages;
  size_t m_phys_pages_count;
  size_t m_total_avail_memory_bytes;
  size_t m_total_unavail_memory_bytes;

  list_t* m_phys_ranges;
  list_t* m_contiguous_ranges;
  list_t* m_used_mem_ranges;


} kmem_data_t;

static kmem_data_t KMEM_DATA;

/*
 *  bitmap page allocator for 4KiB pages
 *  TODO: put this crap into a structure -_-
 */
static volatile uint32_t *frames;
static size_t nframes;
static size_t total_memory = 0;
static size_t unavailable_memory = 0;
static uintptr_t lowest_available = 0;

/*
 *   heap crap
 */
static char *heap_start = NULL;

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
  KMEM_DATA.m_mmap_entry_num = (mmap->size) / mmap->entry_size;
  KMEM_DATA.m_mmap_entries = (struct multiboot_mmap_entry *)mmap->entries;
}

// this layout is inspired by taoruos
#define __def_pagemap __attribute__((aligned(0x1000UL))) = {0}
#define STANDARD_PD_ENTRIES 512

pml_t base_init_pml[3][STANDARD_PD_ENTRIES] __def_pagemap;

extern pml_t kernel_pd[STANDARD_PD_ENTRIES];
extern pml_t kernel_img_pts[STANDARD_PD_ENTRIES * (32 & ENTRY_MASK)];
// could be used for temporary mappings?
extern pml_t kernel_pt_last[STANDARD_PD_ENTRIES];

// TODO: page directory and range abstraction and stuff lol
void init_kmem_manager(uintptr_t* mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {

  KMEM_DATA.m_contiguous_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_phys_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_used_mem_ranges = kmalloc(sizeof(list_t));

  // Step 1: create kernel_pd and shit, then map shit we need

  // Step 2: parse memmap and find physical ranges in memory

  // Step 3: construct physical and virtual pages and their arrays (find a range that can hold our shit and split it in a correct manner)

  // Step 4: 
  
  // we are able to control the layout of virtual addresses with this shit

  uintptr_t virt_kernel_start = HIGH_MAP_BASE | (uintptr_t)&_kernel_start;
  uintptr_t virt_kernel_map_base = virt_kernel_start & ~PDP_MASK;
  
  boot_pdpt[(virt_kernel_map_base >> 30) & 0x1ffu].raw_bits = (uintptr_t)&kernel_pd | 0x3;

  for (uintptr_t i = virt_kernel_start; i <= virt_kernel_start + (uintptr_t)&_kernel_end; i += SMALL_PAGE_SIZE * 512) {

    print("pd_idx: ");
    println(to_string((i - virt_kernel_map_base) >> 21));

    uintptr_t kernel_pts_idx = (i - virt_kernel_start) >> 12;
    kernel_pd[((i - virt_kernel_map_base) >> 21) & 0x1ffu].raw_bits = (uintptr_t)(&kernel_img_pts[kernel_pts_idx]) | 0x3;

    print("pt_idx: ");
    println(to_string((i - virt_kernel_start) >> 12));

    for (uintptr_t j = 0; j < 512; j++) {
      kernel_img_pts[kernel_pts_idx + j].raw_bits = ((uintptr_t)&_kernel_start + (i - virt_kernel_start) + j * SMALL_PAGE_SIZE) | 0x3;
      print("pts_idx: ");
      print(to_string(kernel_pts_idx + j));
      print("  map_addr: ");
      println(to_string((uintptr_t)&_kernel_start + (i - virt_kernel_start) +  j * SMALL_PAGE_SIZE));
    }
  }

  kernel_pd[511].raw_bits = (uintptr_t)&kernel_pt_last | 0x3;

  memset(&kernel_pt_last, 0, sizeof(pml_t) * STANDARD_PD_ENTRIES);

  //for (uintptr_t i = (uintptr_t)&_kernel_start; i < (uintptr_t)&_kernel_end; i++) {
  //. boot_pd0[i] = 0;
  //}
  memset(&boot_pd, 0, sizeof(pml_t) * STANDARD_PD_ENTRIES);

  print("funnies 1: ");
  println(to_string(virt_kernel_start));
  print("funnies 2: ");
  println(to_string(virt_kernel_map_base));
  print("funnies 3: ");
  println(to_string((virt_kernel_map_base >> 30) & 0x1ffu));

  asm volatile ( "movq %0, %%cr3" :: "r"(boot_pml4t) : "memory" );

  struct multiboot_tag_mmap *mmap = get_mb2_tag((void *)mb_addr, 6);
  prep_mmap(mmap);
  parse_memmap();

  kmem_init_physical_allocator();  

  uintptr_t ptr = virt_kernel_start;

  uintptr_t p_ptr = (uintptr_t)&_kernel_start;

  pml_t table = kernel_img_pts[(ptr >> 12) & 0x1ffu];

  println(to_string(table.structured_bits.page << 12));

  println(to_string((uintptr_t)(*(uintptr_t*)p_ptr)));

}

// function inspired by serenityOS
void parse_memmap() {

  phys_mem_range_t kernel_range = {
    .type = PMRT_USABLE,
    .start = (uint64_t)&_kernel_start,
    .length = (uint64_t)(&_kernel_end - &_kernel_start)
  };

  phys_mem_range_t* ptr = kmalloc(sizeof(phys_mem_range_t));
  memcpy(&kernel_range, ptr, sizeof(phys_mem_range_t));
  list_append(KMEM_DATA.m_used_mem_ranges, ptr);


  // kernel should start at 1 Mib
  print("Kernel start addr: ");
  println(to_string((uint64_t)&_kernel_start));
  print("Kernel end addr: ");
  println(to_string((uint64_t)&_kernel_end));

  list_t* contiguous_ranges = kmalloc(sizeof(list_t));
  memset(contiguous_ranges, 0, sizeof(list_t));

  for (uintptr_t i = 0; i < KMEM_DATA.m_mmap_entry_num; i++) {
    multiboot_memory_map_t *map = &KMEM_DATA.m_mmap_entries[i];

    uint64_t addr = map->addr;
    uint64_t length = map->len;
    
    // hihi data
    print("map entry start addr: ");
    println(to_string(addr));
    print("map entry length: ");
    println(to_string(length));
    
    // TODO: register physical memory ranges
    switch (map->type) {
      case (MULTIBOOT_MEMORY_AVAILABLE): {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_USABLE;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_ACPI_RECLAIMABLE): {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_ACPI_RECLAIM;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_NVS): {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_ACPI_NVS;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_RESERVED): {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_RESERVED;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_BADRAM): {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_BAD_MEM;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      default: {
        phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
        range->type = PMRT_UNKNOWN;
        range->start = addr;
        range->length = length;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
    }
    
    if (map->type != MULTIBOOT_MEMORY_AVAILABLE) {
      continue;
    }

    uintptr_t diff = addr % SMALL_PAGE_SIZE;
    if (diff != 0) {
      println("missaligned region!");
      diff = SMALL_PAGE_SIZE - diff;
      addr += diff;
      length -= diff;
    }
    if ((length % SMALL_PAGE_SIZE) != 0) {
      println("missaligned length!");
      length -= length % SMALL_PAGE_SIZE;
    }
    if (length < SMALL_PAGE_SIZE) {
      println("page is too small!");
    }
    
    for (uint64_t page_base = addr; page_base <= (addr + length); page_base += SMALL_PAGE_SIZE) {

      if (page_base >= kernel_range.start && page_base <= kernel_range.start + kernel_range.length) {
        continue;
      }

      // FIXME: how fucked is this going to be for our heap?
      // create a contiguous range by shifting the upper addr by one pagesize every time
      if (contiguous_ranges->end == nullptr || (((contiguous_phys_virt_range_t*)contiguous_ranges->end->data)->upper + SMALL_PAGE_SIZE) != page_base) {
        contiguous_phys_virt_range_t* range = kmalloc(sizeof(contiguous_phys_virt_range_t));
        range->upper = page_base;
        range->lower = page_base;
        list_append(contiguous_ranges, range);
      } else {
        ((contiguous_phys_virt_range_t*)contiguous_ranges->end->data)->upper = page_base;
      }
    }
  }

  size_t total_contig_size = 0;
  node_t* itterator = contiguous_ranges->head;
  while (itterator) {
    contiguous_phys_virt_range_t* range = (contiguous_phys_virt_range_t*)itterator->data;
    list_append(KMEM_DATA.m_contiguous_ranges, range);
    total_contig_size += (range->upper - range->lower);
    itterator = itterator->next;
  }
  kfree(contiguous_ranges);

  KMEM_DATA.m_phys_pages_count = ALIGN_UP(total_contig_size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

  print("Total contiguous pages: ");
  println(to_string(KMEM_DATA.m_phys_pages_count));
}

void kmem_init_physical_allocator() {

  uintptr_t hightest_useable_addr = 0;
  // loop over used shit
  node_t* itterator = KMEM_DATA.m_used_mem_ranges->head;
  while (itterator) {
    phys_mem_range_t* range = (phys_mem_range_t*)(itterator->data);

    uintptr_t range_end = range->start + range->length;

    if (range_end > hightest_useable_addr)
      hightest_useable_addr = range_end;

    itterator = itterator->next;
  }
  // loop over general physical shit
  itterator = KMEM_DATA.m_phys_ranges->head;
  while (itterator) {
    phys_mem_range_t* range = (phys_mem_range_t*)(itterator->data);

    uintptr_t range_end = range->start + range->length;

    if (range_end > hightest_useable_addr)
      hightest_useable_addr = range_end;

    itterator = itterator->next;
  }

  println(to_string(hightest_useable_addr));
  size_t physical_pages_count = kmem_get_page_idx(hightest_useable_addr) + 1;

  // TODO: uhm, what exactly do we use to indicate physical pages?
  size_t physical_pages_bytes = physical_pages_count * sizeof(uint8_t);
  size_t physical_pages_pagecount = ALIGN_UP(physical_pages_bytes, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;
  size_t physical_pagetables_count = (physical_pages_pagecount + 512 - 1) / 512;

  size_t total_physical_count = physical_pages_pagecount + physical_pagetables_count;

  // now where do we put this crap

  contiguous_phys_virt_range_t* useable_range = nullptr;

  itterator = KMEM_DATA.m_contiguous_ranges->head;
  while (itterator) {

    contiguous_phys_virt_range_t* range = (contiguous_phys_virt_range_t*)itterator->data;

    size_t range_size = ALIGN_UP((size_t)(range->upper - range->lower), SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

    if (range_size >= total_physical_count) {
      useable_range = range;
      println("Found range!");
      break;
    }

    itterator = itterator->next;
  }

  println(to_string(total_physical_count));
  println(to_string(KMEM_DATA.m_phys_pages_count));

  // TODO: error
  if (KMEM_DATA.m_phys_pages_count <= total_physical_count) {
    println("Error: too much shit needed to construct physical pages");
    return;
  }

  KMEM_DATA.m_phys_pages_count -= total_physical_count;

  // yoink region
  contiguous_phys_virt_range_t new_range = {
    .lower = useable_range->lower,
    .upper = useable_range->lower + total_physical_count * SMALL_PAGE_SIZE
  };

  useable_range->lower += total_physical_count * SMALL_PAGE_SIZE + 1;

  contiguous_phys_virt_range_t* _ptr = kmalloc(sizeof(contiguous_phys_virt_range_t));
  memcpy(&new_range, _ptr, sizeof(contiguous_phys_virt_range_t));


  println("Append");
  list_append(KMEM_DATA.m_used_mem_ranges, _ptr);

  // first allocate the pagetables
  uintptr_t range_base = new_range.lower;
  uintptr_t phys_pages_base = range_base + physical_pagetables_count * SMALL_PAGE_SIZE;

  println(to_string(physical_pagetables_count));

  quick_print_node_sizes();


}

void *kmem_from_phys(uintptr_t addr) { 
  return (void *)(addr | HIGH_MAP_BASE);
}

uintptr_t kmem_to_phys(pml_t *root, uintptr_t addr) {
  return 0;
}

// flip a bit to 1 as to mark a pageframe as used in our bitmap
void kmem_mark_frame_used(uintptr_t frame) {
}

// flip a bit to 0 as to mark a pageframe as free in our bitmap
void kmem_mark_frame_free(uintptr_t frame) {
  
}

// combination of the above two
void kmem_mark_frame(uintptr_t frame, bool value) {
  
}

uintptr_t kmem_get_frame() {
  return 0; 
}


pml_t *kmem_get_krnl_dir() {
  return nullptr;
}

pml_t *kmem_get_page(uintptr_t addr, unsigned int kmem_flags) {
  return nullptr; 
}


void kmem_nuke_pd(uintptr_t vaddr) { asm volatile("invlpg %0" ::"m"(vaddr) : "memory"); }

// simpler version of the massive chonker above
void _kmem_map_memory(pml_t *page, uintptr_t paddr, unsigned int flags) {
  
}

bool kmem_map_mem(uintptr_t virt, uintptr_t phys, unsigned int flags) {
  return false;
}

bool kmem_map_range(uintptr_t virt_base, uintptr_t phys_base, size_t page_count,
                    unsigned int flags) {
  return true;
}

void kmem_set_page_flags(pml_t *page, unsigned int flags) {
  if (page->structured_bits.page == 0) {
    // TODO:
  }

  page->structured_bits.size = 0;
  page->structured_bits.present_bit = 1;
  page->structured_bits.writable_bit = (flags & KMEM_FLAG_WRITABLE) ? 1 : 0;
  page->structured_bits.user_bit = (flags & KMEM_FLAG_KERNEL) ? 0 : 1;
  page->structured_bits.nocache_bit = (flags & KMEM_FLAG_NOCACHE) ? 1 : 0;
  page->structured_bits.writethrough_bit = (flags & KMEM_FLAG_WRITETHROUGH) ? 1 : 0;
  page->structured_bits.size = (flags & KMEM_FLAG_SPEC) ? 1 : 0;
  page->structured_bits.nx = (flags & KMEM_FLAG_NOEXECUTE) ? 1 : 0;
}


/* OLD IMPL */ 
// 'expand' the kernel heap with a size that is a multiple of a pagesize (so
// basically allocate a number of pages for the heap)

/* NEW IMPL */
// this function should try and find a range of pages that satisfies the requested size
// other name for function: request_pages
void *kmem_alloc(size_t page_count) {
  // find free page


  uintptr_t free_page = kmem_get_frame();
  


  return nullptr;
}

inline uintptr_t kmem_get_page_idx (uintptr_t page_addr) {
  return (page_addr >> 12);
}
inline uintptr_t kmem_get_page_base (uintptr_t page_addr) {
  return (page_addr & ~(0xffful));
}

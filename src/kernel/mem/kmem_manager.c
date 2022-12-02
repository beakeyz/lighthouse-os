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

  volatile uint32_t* m_physical_pages;
  size_t m_phys_pages_count;
  size_t m_total_avail_memory_bytes;
  size_t m_total_unavail_memory_bytes;

  list_t* m_phys_ranges;
  list_t* m_contiguous_ranges;


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
  KMEM_DATA.m_mmap_entry_num = (mmap->size - sizeof(*mmap)) / mmap->entry_size;
  KMEM_DATA.m_mmap_entries = (struct multiboot_mmap_entry *)mmap->entries;
}

// this layout is inspired by taoruos
#define __def_pagemap __attribute__((aligned(0x1000UL))) = {0}
#define standard_pd_entries 512

pml_t base_init_pml[3][standard_pd_entries] __def_pagemap;

extern pml_t kernel_pd[standard_pd_entries];
extern pml_t kernel_pt0[standard_pd_entries];
extern pml_t kernel_img_pts[standard_pd_entries * (32 & ENTRY_MASK)];
extern pml_t kernel_pt_last[standard_pd_entries];

// TODO: page directory and range abstraction and stuff lol
void init_kmem_manager(uintptr_t* mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {

  KMEM_DATA.m_contiguous_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_phys_ranges = kmalloc(sizeof(list_t));

  // Step 1: create kernel_pd and shit, then map shit we need

  // Step 2: parse memmap and find physical ranges in memory

  // Step 3: construct physical and virtual pages and their arrays (find a range that can hold our shit and split it in a correct manner)

  // Step 4: 
  
  // we are able to control the layout of virtual addresses with this shit
  uintptr_t virt_kernel_start = 0x2000000000 | (uintptr_t)&_kernel_start;
  uintptr_t virt_kernel_map_base = virt_kernel_start & ~0x3fffffff;
  
  boot_pdpt[(virt_kernel_map_base >> 30) & 0x1ffu].raw_bits = (uintptr_t)&kernel_pd | 0x3;

  kernel_pd[0].raw_bits = (uintptr_t)&kernel_pt0 | 0x3;

  for (uintptr_t i = virt_kernel_start; i <= virt_kernel_start + (uintptr_t)&_kernel_end; i += PAGE_SIZE) {
    print("pd_idx: ");
    println(to_string((i - virt_kernel_map_base) >> 21));

    uintptr_t kernel_pts_idx = (i - virt_kernel_start) >> 12;
    kernel_pd[(i - virt_kernel_map_base) >> 21].raw_bits = (uintptr_t)(&kernel_img_pts[kernel_pts_idx]) | 0x3;

    print("pt_idx: ");
    println(to_string((i - virt_kernel_start) >> 12));

    for (uintptr_t j = 0; j < 512; j++) {
      kernel_img_pts[kernel_pts_idx + j].raw_bits = ((uintptr_t)&_kernel_start + i + j * SMALL_PAGE_SIZE) | 0x3;
      print("pts_idx: ");
      print(to_string(kernel_pts_idx + j));
      print("  map_addr: ");
      println(to_string((uintptr_t)&_kernel_start + i + j * SMALL_PAGE_SIZE));
    }
  }

  kernel_pd[511].raw_bits = (uintptr_t)&kernel_pt_last | 0x3;

  print("funnies 1: ");
  println(to_string(virt_kernel_start));
  print("funnies 2: ");
  println(to_string(virt_kernel_map_base));
  print("funnies 3: ");
  println(to_string((virt_kernel_map_base >> 30) & 0x1ffu));

  uintptr_t cr3_buffer = 0;
  asm volatile (
    "movq %%cr3, %0\n"
    "movq %0, %%cr3" 
    :: "r"(cr3_buffer) : "memory"
  );

  struct multiboot_tag_mmap *mmap = get_mb2_tag((void *)mb_addr, 6);
  prep_mmap(mmap);
  parse_memmap();

  
}

// function inspired by serenityOS
void parse_memmap() {

  // TODO: currently the only 'used range' is the kernel range, but if this
  // changes, we should compensate for that
  phys_mem_range_t kernel_range = {PMRT_USABLE, (uint64_t)&_kernel_start,
                                   (uint64_t)(&_kernel_end - &_kernel_start)};

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

  print("Total contiguous size: ");
  println(to_string(total_contig_size));

  quick_print_node_sizes();
}

void *kmem_from_phys(uintptr_t addr) { return (void *)(addr | HIGH_MAP_BASE); }

uintptr_t kmem_to_phys(pml_t *root, uintptr_t addr) {
  if (!root)
    root = base_init_pml[0];

  uintptr_t page_addr = (addr & 0xFFFFffffFFFFUL) >> 12;
  uint_t pml4_e = (page_addr >> 27) & ENTRY_MASK;
  uint_t pdp_e = (page_addr >> 18) & ENTRY_MASK;
  uint_t pd_e = (page_addr >> 9) & ENTRY_MASK;
  uint_t pt_e = (page_addr)&ENTRY_MASK;

  if (!root[pml4_e].structured_bits.present_bit)
    return -1;

  println("funnie");
  pml_t *pdp =
      kmem_from_phys((uintptr_t)root[pml4_e].structured_bits.page << 12);

  if (!pdp[pdp_e].structured_bits.present_bit) {
    return (uintptr_t)-2;
  }

  if (pdp[pdp_e].structured_bits.size)
    return ((uintptr_t)pdp[pdp_e].structured_bits.page << 12) |
           (addr & PDP_MASK);

  pml_t *pd = kmem_from_phys((uintptr_t)pdp[pdp_e].structured_bits.page << 12);

  if (!pd[pd_e].structured_bits.present_bit) {
    return (uintptr_t)-3;
  }
  if (pd[pd_e].structured_bits.size)
    return ((uintptr_t)pd[pd_e].structured_bits.page << 12) | (addr & PD_MASK);

  pml_t *pt = kmem_from_phys((uintptr_t)pd[pd_e].structured_bits.page << 12);

  if (!pt[pt_e].structured_bits.present_bit) {
    return (uintptr_t)-4;
  }
  return ((uintptr_t)pt[pt_e].structured_bits.page << 12) | (addr & PT_MASK);
}

// flip a bit to 1 as to mark a pageframe as used in our bitmap
void kmem_mark_frame_used(uintptr_t frame) {
  if (frame < nframes * SMALL_PAGE_SIZE) {
    uintptr_t real_frame = frame >> 12;
    uintptr_t idx = INDEX_FROM_BIT(real_frame);
    uint32_t bit = OFFSET_FROM_BIT(real_frame);
    frames[idx] |= ((uint32_t)1 << bit);
    asm("" ::: "memory");
  }
}

// flip a bit to 0 as to mark a pageframe as free in our bitmap
void kmem_mark_frame_free(uintptr_t frame) {
  if (frame < nframes * SMALL_PAGE_SIZE) {
    uintptr_t real_frame = frame >> 12;
    uintptr_t idx = INDEX_FROM_BIT(real_frame);
    uint32_t bit = OFFSET_FROM_BIT(real_frame);

    frames[idx] &= ~((uint32_t)1 << bit);
    asm("" ::: "memory");
    if (frame < lowest_available)
      lowest_available = frame;
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
      asm("" ::: "memory");
      return;
    }

    frames[idx] &= ~((uint32_t)1 << bit);
    asm("" ::: "memory");
    if (frame < lowest_available)
      lowest_available = frame;
  }
}

uintptr_t kmem_get_frame() {
  for (uintptr_t i = INDEX_FROM_BIT(lowest_available); i < INDEX_FROM_BIT(nframes); i++) {
    if (frames[i] != (uint32_t)-1) {
      for (uintptr_t j = 0; j < (sizeof(uint32_t) * 8); ++j) {
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


pml_t *kmem_get_krnl_dir() {
  return kmem_from_phys((uintptr_t)&base_init_pml[0]);
}

pml_t *kmem_get_page(uintptr_t addr, unsigned int kmem_flags) {
  uintptr_t page_addr = (addr & 0xFFFFffffFFFFUL) >> 12;
  uint_t pml4_e = (page_addr >> 27) & ENTRY_MASK;
  uint_t pdp_e = (page_addr >> 18) & ENTRY_MASK;
  uint_t pd_e = (page_addr >> 9) & ENTRY_MASK;
  uint_t pt_e = (page_addr)&ENTRY_MASK;

  if (!base_init_pml[0][pml4_e].structured_bits.present_bit) {
    println("new pml4");
    if (kmem_flags & KMEM_GET_MAKE) {
      uintptr_t new = kmem_get_frame() << 12;
      kmem_mark_frame_used(new);
      memset(kmem_from_phys(new), 0, SMALL_PAGE_SIZE);
      base_init_pml[0][pml4_e].raw_bits = new | 0x07;
    } else {
      return nullptr;
    }
  }

  pml_t *pdp =
      kmem_from_phys(base_init_pml[0][pml4_e].structured_bits.page << 12);

  if (!pdp[pdp_e].structured_bits.present_bit) {
    println("new pdp");
    if (kmem_flags & KMEM_GET_MAKE) {
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

  pml_t *pd = kmem_from_phys(pdp[pdp_e].structured_bits.page << 12);

  if (!pd[pd_e].structured_bits.present_bit) {
    println("new pd");
    if (kmem_flags & KMEM_GET_MAKE) {
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

  pml_t *pt = kmem_from_phys(pd[pd_e].structured_bits.page << 12);
  return (pml_t *)&pt[pt_e];
}


void kmem_nuke_pd(uintptr_t vaddr) { asm volatile("invlpg %0" ::"m"(vaddr) : "memory"); }

// simpler version of the massive chonker above
void _kmem_map_memory(pml_t *page, uintptr_t paddr, unsigned int flags) {
  if (page) {
    kmem_mark_frame_used(paddr);
    page->structured_bits.page = paddr >> 12;
    kmem_set_page_flags(page, flags);
  }
}

bool kmem_map_mem(uintptr_t virt, uintptr_t phys, unsigned int flags) {
  if (virt % SMALL_PAGE_SIZE == 0) {
    pml_t *page = kmem_get_page(virt, KMEM_GET_MAKE);
    _kmem_map_memory(page, phys, flags);
    return true;
  }
  return false;
}

bool kmem_map_range(uintptr_t virt_base, uintptr_t phys_base, size_t page_count,
                    unsigned int flags) {
  if (page_count % page_count != 0) {
    return false;
  }

  for (uintptr_t i = 0; i <= page_count; ++i) {
    uintptr_t mult = i * SMALL_PAGE_SIZE;
    kmem_map_mem(virt_base + mult, phys_base + mult, flags);
  }
  return true;
}

void kmem_set_page_flags(pml_t *page, unsigned int flags) {
  if (page->structured_bits.page == 0) {
    uintptr_t frame = kmem_get_frame();
    kmem_mark_frame_used(frame << 12);
    page->structured_bits.page = frame;
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

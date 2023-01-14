#include "kmem_manager.h"
#include "interupts/interupts.h"
#include "kernel/dev/debug/serial.h"
#include "kernel/kmain.h"
#include "kernel/mem/kmalloc.h"
#include "kernel/mem/PagingComplex.h"
#include "kernel/libk/multiboot.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/stddef.h>
#include <libk/string.h>

// this layout is inspired by taoruos
#define __def_pagemap __attribute__((aligned(0x1000UL))) = {0}
#define STANDARD_PD_ENTRIES 512
#define MAX_RETRIES_FOR_PAGE_MAPPING 5

typedef struct {
  uint32_t m_mmap_entry_num;
  multiboot_memory_map_t* m_mmap_entries;
  uint8_t m_reserved_phys_count;

  bitmap_t m_phys_bitmap;
  uintptr_t m_highest_phys_addr;
  size_t m_phys_pages_count;
  size_t m_total_avail_memory_bytes;
  size_t m_total_unavail_memory_bytes;

  list_t* m_phys_ranges;
  list_t* m_contiguous_ranges;
  list_t* m_used_mem_ranges;

  PagingComplex_t m_kernel_base_pd[3][512] __attribute__((aligned(0x1000UL)));
  PagingComplex_t m_high_base_pd[512] __attribute__((aligned(0x1000UL)));
  PagingComplex_t m_high_base_pts[64][512] __attribute__((aligned(0x1000UL)));

  PagingComplex_t m_phys_base_pd[512] __attribute__((aligned(0x1000UL)));
  PagingComplex_t m_phys_base_pts[64][512] __attribute__((aligned(0x1000UL)));

} kmem_data_t;

static kmem_data_t KMEM_DATA;

// TODO: move to processor struct

// external directory layout
extern PagingComplex_t kernel_pd[STANDARD_PD_ENTRIES];
extern PagingComplex_t kernel_img_pts[32][STANDARD_PD_ENTRIES];
// could be used for temporary mappings?
extern PagingComplex_t kernel_pt_last[STANDARD_PD_ENTRIES];

static inline void _init_kmem_page_layout();
static inline void _load_page_dir(uintptr_t dir, bool __disable_interupts);

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
  KMEM_DATA.m_mmap_entry_num = (mmap->size - sizeof(struct multiboot_tag_mmap*)) / mmap->entry_size;
  KMEM_DATA.m_mmap_entries = (struct multiboot_mmap_entry *)mmap->entries;
}

/*
*  TODO: A few things need to be figured out:
*           - (Optional, but desireable) Redo the PagingComplex_t structure to be more verbose and straight forward
*           - Start to think about optimisations
*           - more implementations
*/
void init_kmem_manager(uintptr_t* mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {

  KMEM_DATA.m_contiguous_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_phys_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_used_mem_ranges = kmalloc(sizeof(list_t));

  memset(&KMEM_DATA.m_kernel_base_pd, 0, sizeof(PagingComplex_t) * 3 * 512);

  // nested fun
  prep_mmap(get_mb2_tag((void *)mb_addr, 6));
  parse_memmap();

  kmem_init_physical_allocator();  

  _init_kmem_page_layout();

  
  // FIXME: find out if this address is always valid
  uintptr_t map = ((uintptr_t)(PagingComplex_t *)&KMEM_DATA.m_kernel_base_pd[0]);
 
  _load_page_dir(map, true);

}

// TODO: remove, once we don't need this anymore for emergency debugging

void print_bitmap () {

  for (uintptr_t i = 0; i < KMEM_DATA.m_phys_bitmap.m_entries; i++) {
    if (KMEM_DATA.m_phys_bitmap.fIsSet(&KMEM_DATA.m_phys_bitmap, i)) {
      print("1");
    } else {
      print("0");
    }
  }
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
    
    // TODO: register physical memory ranges
    phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
    range->start = addr;
    range->length = length;
    switch (map->type) {
      case (MULTIBOOT_MEMORY_AVAILABLE): {
        range->type = PMRT_USABLE;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_ACPI_RECLAIMABLE): {
        range->type = PMRT_ACPI_RECLAIM;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_NVS): {
        range->type = PMRT_ACPI_NVS;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_RESERVED): {
        range->type = PMRT_RESERVED;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      case (MULTIBOOT_MEMORY_BADRAM): {
        range->type = PMRT_BAD_MEM;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
      default: {
        range->type = PMRT_UNKNOWN;
        list_append(KMEM_DATA.m_phys_ranges, range);
        break;
      }
    }
    
    if (map->type != MULTIBOOT_MEMORY_AVAILABLE) {
      continue;
    }

    // hihi data
    print("map entry start addr: ");
    println(to_string(addr));
    print("map entry length: ");
    println(to_string(length));

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
      println("bruh, bootloader gave us a region smaller than a pagesize ;-;");
    }

    uintptr_t highest_addr = addr + length;
    if (highest_addr > KMEM_DATA.m_highest_phys_addr) {
      KMEM_DATA.m_highest_phys_addr = highest_addr;
    }
    
    for (uint64_t page_base = addr; page_base <= (addr + length); page_base += SMALL_PAGE_SIZE) {

      if (page_base >= kernel_range.start && page_base <= kernel_range.start + kernel_range.length) {
        continue;
      }

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

  // devide by pagesize
  KMEM_DATA.m_phys_pages_count = ALIGN_UP(total_contig_size, SMALL_PAGE_SIZE) >> 12;

  print("Total contiguous pages: ");
  println(to_string(KMEM_DATA.m_phys_pages_count));
}

void kmem_init_physical_allocator() {

  size_t physical_pages_bytes = (KMEM_DATA.m_phys_pages_count + 8 - 1) >> 3;

  // FIXME: we should probably move the bitmap somewhere else later, since
  // we need to map the heap to a virtual addressspace eventually. This means
  // that the heap might get reinitiallised and thus we lose our bitmap...
  // just keep this in mind
  bitmap_t map = init_bitmap(physical_pages_bytes);
  memcpy(&KMEM_DATA.m_phys_bitmap, &map, sizeof(map));

  // Mark the contiguous 'free' ranges as free in our bitmap
  node_t* itterator = KMEM_DATA.m_contiguous_ranges->head;
  while (itterator) {

    contiguous_phys_virt_range_t* range = itterator->data;

    // should already be page aligned
    for (uintptr_t addr = ALIGN_UP(range->lower, SMALL_PAGE_SIZE); addr < ALIGN_DOWN(range->upper, SMALL_PAGE_SIZE); addr += SMALL_PAGE_SIZE) {
      uintptr_t phys_page_idx = kmem_get_page_idx(addr);
      kmem_set_phys_page_free(phys_page_idx);
    }

    itterator = itterator->next;
  }

  for (uintptr_t i = (uintptr_t)&_kernel_start; i < (uintptr_t)&_kernel_end; i += SMALL_PAGE_SIZE) {
    uintptr_t physical_page_index = kmem_get_page_idx(i);
    kmem_set_phys_page_used(physical_page_index);
  }
}

void *kmem_from_phys(uintptr_t addr) { 
  return (void *)(addr | HIGH_MAP_BASE);
}

uintptr_t kmem_to_phys(PagingComplex_t *root, uintptr_t addr) {
  return 0;
}

// flip a bit to 1 as to mark a pageframe as used in our bitmap
void kmem_set_phys_page_used(uintptr_t idx) {
  KMEM_DATA.m_phys_bitmap.fMark(&KMEM_DATA.m_phys_bitmap, idx);
}

// flip a bit to 0 as to mark a pageframe as free in our bitmap
void kmem_set_phys_page_free(uintptr_t idx) {
  KMEM_DATA.m_phys_bitmap.fUnmark(&KMEM_DATA.m_phys_bitmap, idx);
}

bool kmem_is_phys_page_used (uintptr_t idx) {
  const bitmap_t* __map = &KMEM_DATA.m_phys_bitmap;
  return __map->fIsSet(&KMEM_DATA.m_phys_bitmap, idx);
}

// combination of the above two
// value = true -> marks
// value = false -> unmarks
void kmem_set_phys_page(uintptr_t idx, bool value) {
  if (value) {
    KMEM_DATA.m_phys_bitmap.fMark(&KMEM_DATA.m_phys_bitmap, idx);
  } else {
    KMEM_DATA.m_phys_bitmap.fUnmark(&KMEM_DATA.m_phys_bitmap, idx);
  }
}

// TODO: errorhandle
ErrorOrPtr kmem_request_pysical_page() {

  uintptr_t index = KMEM_DATA.m_phys_bitmap.fFindFree(&KMEM_DATA.m_phys_bitmap);

  if (index == NULL) {
    return Error();
  }

  return Success(index << 12);
}

// TODO: errorhandle
ErrorOrPtr kmem_prepare_new_physical_page() {
  // find
  ErrorOrPtr result = kmem_request_pysical_page();

  if (result.m_status == LIGHT_SUCCESS) {
    uintptr_t new = result.m_ptr;
    // set
    kmem_set_phys_page_used(kmem_get_page_idx(new));
    // zero
    memset(kmem_from_phys(new), 0x00, SMALL_PAGE_SIZE);
  } else {
    // clean?
    println("failed to request page");
  }

  return result;
}

PagingComplex_t *kmem_get_krnl_dir() {
  return (PagingComplex_t *)&KMEM_DATA.m_kernel_base_pd[0];
}

PagingComplex_t *kmem_get_page(PagingComplex_t* root, uintptr_t addr, unsigned int kmem_flags) {
  const uintptr_t pml4_idx = (addr >> 39) & ENTRY_MASK;
  const uintptr_t pdp_idx = (addr >> 30) & ENTRY_MASK;
  const uintptr_t pd_idx = (addr >> 21) & ENTRY_MASK;
  const uintptr_t pt_idx = (addr >> 12) & ENTRY_MASK;
  const bool should_make = (kmem_flags & KMEM_CUSTOMFLAG_GET_MAKE);
  const bool should_make_user = (kmem_flags & KMEM_CUSTOMFLAG_CREATE_USER);
  const uintptr_t page_creation_flags = (should_make_user ? 0x7 : 0x3);

  uint32_t tries = MAX_RETRIES_FOR_PAGE_MAPPING;
  for ( ; ; ) {
    if (tries == 0) {
      break;
    }

    PagingComplex_t* pml4 = (root == nullptr ? (PagingComplex_t*)&KMEM_DATA.m_kernel_base_pd[0] : root);
    const bool pml4_entry_exists = (pml4[pml4_idx].pde_bits.present_bit);

    if (!pml4_entry_exists) {
      if (should_make) {
        uintptr_t addr = kmem_prepare_new_physical_page().m_ptr;
        pml4[pml4_idx].raw_bits = addr | page_creation_flags;

        tries--;
        continue;
      }
      return nullptr;
    }

    PagingComplex_t* pdp = kmem_from_phys((uintptr_t)pml4[pml4_idx].pde_bits.page << 12);
    const bool pdp_entry_exists = (pdp[pdp_idx].pde_bits.present_bit);

    if (!pdp_entry_exists) {
      if (should_make) {
        uintptr_t addr = kmem_prepare_new_physical_page().m_ptr;
        pdp[pdp_idx].raw_bits = addr | page_creation_flags;
        tries--;
        continue;
      }
      return nullptr;
    }

    PagingComplex_t* pd = kmem_from_phys((uintptr_t)pdp[pdp_idx].pde_bits.page << 12);
    const bool pd_entry_exists = (pd[pd_idx].pde_bits.present_bit);

    if (!pd_entry_exists) {
      if (should_make) {
        uintptr_t addr = kmem_prepare_new_physical_page().m_ptr;
        pd[pd_idx].raw_bits = addr | page_creation_flags;
        tries--;
        continue;
      }
      return nullptr;
    }

    if (pd[pd_idx].pde_bits.size) {
      println("SIZE SET");
      break;
    }

    // this just should exist
    const PagingComplex_t* pt = kmem_from_phys((uintptr_t)pd[pd_idx].pte_bits.page << 12);
    return (PagingComplex_t*)&pt[pt_idx];
  }
  
  println("[WARNING] Could not find page!");
  return (PagingComplex_t*)nullptr; 
}


void kmem_nuke_pd(uintptr_t vaddr) { 
  asm volatile("invlpg %0" ::"m"(vaddr) : "memory");
}

bool kmem_map_page (PagingComplex_t* table, uintptr_t virt, uintptr_t phys, unsigned int flags, uint32_t page_flags) {

  PagingComplex_t* __page = kmem_get_page(table, virt, flags);
  
  if (__page) {
    // TODO: Variablize these flags
    __page->pde_bits.page = kmem_get_page_idx(phys);

    // secure page
    uint32_t __flags = page_flags;
    if (__flags == 0) {
      __flags = KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL;
    }
    kmem_set_page_flags(__page, page_flags);

    return true;
  }
  return false;
}

bool kmem_map_range(uintptr_t virt_base, uintptr_t phys_base, size_t page_count,
                    unsigned int flags) {
  return true;
}

void kmem_set_page_flags(PagingComplex_t *page, unsigned int flags) {
  if (page->pte_bits.page == 0) {
    // TODO:
    uintptr_t idx = kmem_request_pysical_page().m_ptr >> 12;
    page->pte_bits.page = idx;
  }

  //page->structured_bits.size = 0;
  page->pte_bits.present_bit = 1;
  page->pte_bits.writable_bit = (flags & KMEM_FLAG_WRITABLE) ? 1 : 0;
  page->pte_bits.user_bit = (flags & KMEM_FLAG_KERNEL) ? 0 : 1;
  page->pte_bits.nocache_bit = (flags & KMEM_FLAG_NOCACHE) ? 1 : 0;
  page->pte_bits.writethrough_bit = (flags & KMEM_FLAG_WRITETHROUGH) ? 1 : 0;
  page->pte_bits.PAT = (flags & KMEM_FLAG_SPEC) ? 1 : 0;
  page->pte_bits.nx = (flags & KMEM_FLAG_NOEXECUTE) ? 1 : 0;
}

/* NEW IMPL */
// allocates a region using the physical allocator and then 
// identity maps it
void* kmem_kernel_alloc (uintptr_t addr, size_t size, int flags) {

  if (kmem_is_phys_page_used(kmem_get_page_idx(addr)) && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
    return nullptr;
  }

  // find free page
  const size_t pages_needed = (size + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;
  void* ret = (void*)addr;
  uintptr_t __addr = addr;

  for (uintptr_t i = 0; i < pages_needed; i++) {

    const uintptr_t page_idx = kmem_get_page_idx(__addr);
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (was_used && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
      return nullptr;
    }

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(nullptr, __addr, __addr, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (!result) {
      return nullptr;
    }

    __addr += SMALL_PAGE_SIZE;
  }

  return ret;
}

// FIXME: code duplication
// FIXME: pages_needed calculation turned out to spit out waaay more pages than we needed,
// so we need to test if this fix will suffice =/
void* kmem_kernel_alloc_extended (uintptr_t addr, size_t size, int flags, int page_flags) {
  const size_t pages_needed = (size + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;
  void* ret = (void*)addr;
  uintptr_t __addr = addr;

  for (uintptr_t i = 0; i < pages_needed; i++) {

    const uintptr_t page_idx = kmem_get_page_idx(__addr);
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (was_used && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
      return nullptr;
    }

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(nullptr, __addr, __addr, KMEM_CUSTOMFLAG_GET_MAKE, page_flags);

    if (!result) {
      return nullptr;
    }

    __addr += SMALL_PAGE_SIZE;
  }

  return ret;
}

// @params
// size: size in bytes that should be allocated
// TODO: test
ErrorOrPtr kmem_kernel_alloc_range (size_t size, int custom_flags, int page_flags) {
  const size_t pages_needed = (size + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;
  println(to_string(pages_needed));

  const uintptr_t start_idx = Must(KMEM_DATA.m_phys_bitmap.fFindFreeRange(&KMEM_DATA.m_phys_bitmap, pages_needed));
  const uintptr_t ret = kmem_get_page_base(start_idx);

  for (uintptr_t i = 0; i < pages_needed; i++) {
    const uintptr_t page_idx = start_idx + i;
    const uintptr_t addr = kmem_get_page_base(page_idx);
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (was_used && !(custom_flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
      return Error();
    }

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(nullptr, addr, addr, KMEM_CUSTOMFLAG_GET_MAKE, page_flags);


    if (!result) {
      return Error();
    }
  }
  return Success(ret);
}

// TODO: make this more dynamic
static inline void _init_kmem_page_layout () {

  size_t needed_pagedirs_count = (ALIGN_UP(KMEM_DATA.m_phys_pages_count, SMALL_PAGE_SIZE) + 511) >> 9;

  print("NEEDED PAGEDIR COUNT: ");
  println(to_string(needed_pagedirs_count));

  // Map 64 pds to HIGH_MAP_BASE ()
  KMEM_DATA.m_kernel_base_pd[0][511].raw_bits = (uintptr_t)&KMEM_DATA.m_high_base_pd | 0x3;
  for (uintptr_t dir = 0; dir < 64; dir++) {
    KMEM_DATA.m_high_base_pd[dir].raw_bits = (uintptr_t)&KMEM_DATA.m_high_base_pts[dir] | 0x3;
    for (uintptr_t page = 0; page < 512; page++) {
      KMEM_DATA.m_high_base_pts[dir][page].raw_bits = ((dir << 30) + (page << 21)) | 0x83;
    }
  }

  KMEM_DATA.m_kernel_base_pd[0][0].raw_bits = (uintptr_t)&kernel_img_pts[0] | 0x3;

  kernel_img_pts[0][0].raw_bits = (uintptr_t)&kernel_pd[0] | 0x3;

  const uintptr_t virt_kernel_end_ptr = ((uintptr_t)&_kernel_end + PAGE_LOW_MASK) & PAGE_SIZE_MASK;
  const size_t kernel_page_count = virt_kernel_end_ptr >> 12;
  const size_t kernel_pagetable_count = (kernel_page_count + ENTRY_MASK) >> 9;

  for (size_t i = 0; i < kernel_pagetable_count; i++) {
    const size_t current_pt_idx = 2 + i;

    kernel_pd[i].raw_bits = (uintptr_t)&kernel_img_pts[current_pt_idx] | 0x3;
    for (size_t j = 0; j < STANDARD_PD_ENTRIES; j++) {
      kernel_img_pts[current_pt_idx][j].raw_bits = (uintptr_t)(PAGE_SIZE * i + SMALL_PAGE_SIZE * j) | 0x3;
    }
  }
}


static inline void _load_page_dir(uintptr_t dir, bool __disable_interupts) {
  if (__disable_interupts) 
    disable_interupts();

  ASSERT(g_GlobalSystemInfo.m_current_core != nullptr);
  g_GlobalSystemInfo.m_current_core->m_page_dir = *(PagingComplex_t*)dir;

  asm volatile("" : : : "memory");
  asm volatile("movq %0, %%cr3" ::"r"(dir));
  asm volatile("" : : : "memory");
}

// FIXME: macroes?
uintptr_t kmem_get_page_idx (uintptr_t page_addr) {
  return (page_addr >> 12);
}
uintptr_t kmem_get_page_base (uintptr_t page_addr) {
  return (page_addr & ~(0xffful));
}

// deprecated
PagingComplex_t create_pagetable(uintptr_t paddr, bool writable) {
  PagingComplex_t table = {
    .raw_bits = paddr
  };

  table.pde_bits.present_bit = 1;
  table.pde_bits.writable_bit = writable;

  return table;
}

const list_t* kmem_get_phys_ranges_list() {
  return KMEM_DATA.m_phys_ranges;
}

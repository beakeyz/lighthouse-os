#include "kmem_manager.h"
#include "dev/kterm/kterm.h"
#include "interupts/interupts.h"
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include <mem/heap.h>
#include "mem/page_dir.h"
#include "mem/pg.h"
#include "libk/multiboot.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/quickmapper.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/processor/processor.h"
#include <mem/heap.h>
#include <mem/base_allocator.h>
#include <libk/stddef.h>
#include <libk/string.h>

#define STANDARD_PD_ENTRIES 512
#define MAX_RETRIES_FOR_PAGE_MAPPING 5

struct {
  uint32_t m_mmap_entry_num;
  multiboot_memory_map_t* m_mmap_entries;
  uint8_t m_reserved_phys_count;

  bitmap_t* m_phys_bitmap;
  uintptr_t m_highest_phys_addr;
  size_t m_phys_pages_count;
  size_t m_total_avail_memory_bytes;
  size_t m_total_unavail_memory_bytes;

  list_t* m_phys_ranges;
  list_t* m_contiguous_ranges;
  list_t* m_used_mem_ranges;

  pml_entry_t* m_kernel_base_pd;

} KMEM_DATA;

static inline void _init_kmem_page_layout();

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) {
  KMEM_DATA.m_mmap_entry_num = (mmap->size - sizeof(struct multiboot_tag_mmap*)) / mmap->entry_size;
  KMEM_DATA.m_mmap_entries = (struct multiboot_mmap_entry *)mmap->entries;
}

/*
*  TODO: A few things need to be figured out:
*           - (Optional, but desireable) Redo the pml_entry_t structure to be more verbose and straight forward
*           - Start to think about optimisations
*           - more implementations
*           - Implement a lock around the physical allocator
*/
void init_kmem_manager(uintptr_t* mb_addr, uintptr_t first_valid_addr, uintptr_t first_valid_alloc_addr) {

  KMEM_DATA.m_contiguous_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_phys_ranges = kmalloc(sizeof(list_t));
  KMEM_DATA.m_used_mem_ranges = kmalloc(sizeof(list_t));

  // nested fun
  prep_mmap(get_mb2_tag((void *)mb_addr, 6));
  parse_mmap();

  kmem_init_physical_allocator();

  KMEM_DATA.m_kernel_base_pd = (pml_entry_t*)kmem_ensure_high_mapping(kmem_prepare_new_physical_page().m_ptr);

  _init_kmem_page_layout();

  // FIXME: find out if this address is always valid
  // NOTE: If we ever decide to change the boottime mappings, this (along with the 
  // kmem_from_phys usages before switching to the new pagemap) will not be valid anymore...
  uintptr_t map = (uintptr_t)KMEM_DATA.m_kernel_base_pd - HIGH_MAP_BASE;

 
  load_page_dir(map, true);

}

// TODO: remove, once we don't need this anymore for emergency debugging

void print_bitmap () {

  for (uintptr_t i = 0; i < KMEM_DATA.m_phys_bitmap->m_entries; i++) {
    if (bitmap_isset(KMEM_DATA.m_phys_bitmap, i)) {
      print("1");
    } else {
      print("0");
    }
  }
}


// function inspired by serenityOS
void parse_mmap() {

  // ???
  phys_mem_range_t kernel_range = {
    .type = PMRT_USABLE,
    .start = (uint64_t)&_kernel_start - HIGH_MAP_BASE,
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

      if (KMEM_DATA.m_contiguous_ranges->end == nullptr || (((contiguous_phys_virt_range_t*)KMEM_DATA.m_contiguous_ranges->end->data)->upper + SMALL_PAGE_SIZE) != page_base) {
        contiguous_phys_virt_range_t* range = kmalloc(sizeof(contiguous_phys_virt_range_t));
        range->upper = page_base;
        range->lower = page_base;
        list_append(KMEM_DATA.m_contiguous_ranges, range);
      } else {
        ((contiguous_phys_virt_range_t*)KMEM_DATA.m_contiguous_ranges->end->data)->upper = page_base;
      }
    }
  }

  size_t total_contig_size = 0;

  FOREACH(i, KMEM_DATA.m_contiguous_ranges) {
    contiguous_phys_virt_range_t* range = i->data;

    total_contig_size += (range->upper - range->lower);
  }

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
  KMEM_DATA.m_phys_bitmap = create_bitmap(physical_pages_bytes);

  // Mark the contiguous 'free' ranges as free in our bitmap
  FOREACH(i, KMEM_DATA.m_contiguous_ranges) {
    contiguous_phys_virt_range_t* range = i->data;

    paddr_t base = range->lower;
    size_t size = range->upper - range->lower;

    // should already be page aligned
    for (uintptr_t i = 0; i < size >> 12; i++) {
      paddr_t addr = base + i * SMALL_PAGE_SIZE;
      uintptr_t phys_page_idx = kmem_get_page_idx(addr);
      kmem_set_phys_page_free(phys_page_idx);
    }
  }

  println(to_string((uintptr_t)&_kernel_start));
  println(to_string((uintptr_t)&_kernel_end));

  // subtract the base of the mapping used while booting
  paddr_t physical_kernel_start = (uintptr_t)&_kernel_start - HIGH_MAP_BASE;
  paddr_t physical_kernel_end = (uintptr_t)&_kernel_end - HIGH_MAP_BASE;
  size_t kernel_size = physical_kernel_end - physical_kernel_start;

  for (uintptr_t i = 0; i < kernel_size >> 12; i++) {
    paddr_t addr = physical_kernel_start + (i << 12);
    uintptr_t physical_page_index = kmem_get_page_idx(addr);
    kmem_set_phys_page_used(physical_page_index);
  }
}

vaddr_t kmem_ensure_high_mapping(uintptr_t addr) { 
  return (vaddr_t)(addr | HIGH_MAP_BASE);
}

vaddr_t kmem_from_phys (uintptr_t addr, vaddr_t vbase) {
  return (vaddr_t)(addr | vbase);
}

uintptr_t kmem_to_phys_aligned(pml_entry_t* root, uintptr_t addr) {

  pml_entry_t *page = kmem_get_page(root, addr, 0, 0);

  if (page == nullptr) {
    return NULL;
  }

  return kmem_get_page_base(page->raw_bits);
}

uintptr_t kmem_to_phys(pml_entry_t *root, uintptr_t addr) {

  paddr_t aligned_paddr = ALIGN_DOWN(addr, SMALL_PAGE_SIZE);
  size_t delta = addr - aligned_paddr;

  pml_entry_t *page = kmem_get_page(root, addr, 0, 0);

  /* Address is not mapped */
  if (page == nullptr) {
    return NULL;
  }

  paddr_t aligned_base = kmem_get_page_base(page->raw_bits);

  return aligned_base + delta;
}

// flip a bit to 1 as to mark a pageframe as used in our bitmap
void kmem_set_phys_page_used(uintptr_t idx) {
  bitmap_mark(KMEM_DATA.m_phys_bitmap, idx);
}

// flip a bit to 0 as to mark a pageframe as free in our bitmap
void kmem_set_phys_page_free(uintptr_t idx) {
  bitmap_unmark(KMEM_DATA.m_phys_bitmap, idx);
}

bool kmem_is_phys_page_used (uintptr_t idx) {
  return bitmap_isset(KMEM_DATA.m_phys_bitmap, idx);
}

// combination of the above two
// value = true -> marks
// value = false -> unmarks
void kmem_set_phys_page(uintptr_t idx, bool value) {
  if (value) {
    bitmap_mark(KMEM_DATA.m_phys_bitmap, idx);
  } else {
    bitmap_unmark(KMEM_DATA.m_phys_bitmap, idx);
  }
}

// TODO: errorhandle
ErrorOrPtr kmem_request_physical_page() {

  ErrorOrPtr result = bitmap_find_free(KMEM_DATA.m_phys_bitmap);

  if (result.m_status == ANIVA_FAIL) {
    return Error();
  }

  uintptr_t index = result.m_ptr;

  return Success(kmem_get_page_addr(index));
}

// TODO: errorhandle
ErrorOrPtr kmem_prepare_new_physical_page() {
  // find
  TRY(result, kmem_request_physical_page());

  uintptr_t new = result.m_ptr;
  // set
  kmem_set_phys_page_used(kmem_get_page_idx(new));

  // There might be an issue here as we try to zero
  // and this page is never mapped. We might need some
  // sort of temporary mapping like serenity has to do
  // this, but we might also rewrite this entire thing
  // as to map this sucker before we zero. This would 
  // mean that you can't just get a nice clean page...
  memset((void*)kmem_ensure_high_mapping(new), 0x00, SMALL_PAGE_SIZE);

  return result;
}

ErrorOrPtr kmem_return_physical_page(paddr_t page_base) {

  if (ALIGN_UP(page_base, SMALL_PAGE_SIZE) != page_base) {
    return Error();
  }

  vaddr_t vbase = kmem_ensure_high_mapping(page_base);

  uint32_t index = kmem_get_page_idx(page_base);

  kmem_set_phys_page_free(index);

  memset((void*)vbase, 0, SMALL_PAGE_SIZE);

  return Success(0);
}

pml_entry_t *kmem_get_krnl_dir() {
  return (pml_entry_t*)((uintptr_t)KMEM_DATA.m_kernel_base_pd - HIGH_MAP_BASE);
}

pml_entry_t *kmem_get_page(pml_entry_t* root, uintptr_t addr, unsigned int kmem_flags, uint32_t page_flags) {

  if ((kmem_flags & KMEM_CUSTOMFLAG_USE_QUICKMAP) == KMEM_CUSTOMFLAG_USE_QUICKMAP) {
    return kmem_get_page_with_quickmap(root, addr, kmem_flags, page_flags);
  }

  const uintptr_t pml4_idx = (addr >> 39) & ENTRY_MASK;
  const uintptr_t pdp_idx = (addr >> 30) & ENTRY_MASK;
  const uintptr_t pd_idx = (addr >> 21) & ENTRY_MASK;
  const uintptr_t pt_idx = (addr >> 12) & ENTRY_MASK;
  const bool should_make = ((kmem_flags & KMEM_CUSTOMFLAG_GET_MAKE) == KMEM_CUSTOMFLAG_GET_MAKE);
  const bool should_make_user = ((kmem_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER);
  const uintptr_t page_creation_flags = (should_make_user ? 0x07 : 0x03) | page_flags;

  uint32_t tries = MAX_RETRIES_FOR_PAGE_MAPPING;
  for ( ; ; ) {
    if (tries == 0) {
      break;
    }

    pml_entry_t* pml4 = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)(root == nullptr ? kmem_get_krnl_dir() : root));
    const bool pml4_entry_exists = (pml_entry_is_bit_set(&pml4[pml4_idx], PDE_PRESENT));

    if (!pml4_entry_exists) {
      if (should_make) {
        paddr_t page_addr = kmem_prepare_new_physical_page().m_ptr;

        kmem_set_page_base(&pml4[pml4_idx], page_addr);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_PRESENT, true);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_READ_WRITE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
        pml_entry_set_bit(&pml4[pml4_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);

        tries--;
        continue;
      }
      return nullptr;
    }

    pml_entry_t* pdp = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits));
    const bool pdp_entry_exists = (pml_entry_is_bit_set((pml_entry_t*)&pdp[pdp_idx], PDE_PRESENT));

    if (!pdp_entry_exists) {
      if (should_make) {
        uintptr_t page_addr = kmem_prepare_new_physical_page().m_ptr;
        kmem_set_page_base(&pdp[pdp_idx], page_addr);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_PRESENT, true);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_READ_WRITE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
        pml_entry_set_bit(&pdp[pdp_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);
        tries--;
        continue;
      }
      return nullptr;
    }

    pml_entry_t* pd = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits));
    const bool pd_entry_exists = pml_entry_is_bit_set(&pd[pd_idx], PDE_PRESENT);

    if (!pd_entry_exists) {
      if (should_make) {
        uintptr_t page_addr = kmem_prepare_new_physical_page().m_ptr;

        kmem_set_page_base(&pd[pd_idx], page_addr);
        pml_entry_set_bit(&pd[pd_idx], PDE_PRESENT, true);
        pml_entry_set_bit(&pd[pd_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
        pml_entry_set_bit(&pd[pd_idx], PDE_READ_WRITE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
        pml_entry_set_bit(&pd[pd_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
        pml_entry_set_bit(&pd[pd_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
        pml_entry_set_bit(&pd[pd_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
        pml_entry_set_bit(&pd[pd_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);
        tries--;
        continue;
      }
      return nullptr;
    }

    // this just should exist
    const pml_entry_t* pt = (const pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits));
    return (pml_entry_t*)&pt[pt_idx];
  }
  
  println("[WARNING] Could not find page!");
  return (pml_entry_t*)nullptr; 
}

/*
 * NOTE: table needs to be a physical pointer
 */
pml_entry_t* kmem_get_page_with_quickmap (pml_entry_t* table, vaddr_t virt, uint32_t kmem_flags, uint32_t page_flags) {
  const uintptr_t pml4_idx = (virt >> 39) & ENTRY_MASK;
  const uintptr_t pdp_idx = (virt >> 30) & ENTRY_MASK;
  const uintptr_t pd_idx = (virt >> 21) & ENTRY_MASK;
  const uintptr_t pt_idx = (virt >> 12) & ENTRY_MASK;
  const bool should_make = ((kmem_flags & KMEM_CUSTOMFLAG_GET_MAKE) == KMEM_CUSTOMFLAG_GET_MAKE);
  const bool should_make_user = ((kmem_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER);
  const uintptr_t page_creation_flags = (should_make_user ? 0x07 : 0x03) | page_flags;

  pml_entry_t* pml4 = (pml_entry_t*)quick_map((uintptr_t)(table == nullptr ? kmem_get_krnl_dir() : table));
  const bool pml4_entry_exists = ((pml4[pml4_idx].raw_bits & PDE_PRESENT) == PDE_PRESENT);

  if (!pml4_entry_exists) {
    if (!should_make) {
      return nullptr;
    }
    kmem_set_page_flags(&pml4[pml4_idx], page_creation_flags);
  }

  pml_entry_t* pdp = (pml_entry_t*)quick_map((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits));
  const bool pdp_entry_exists = (pml_entry_is_bit_set((pml_entry_t*)&pdp[pdp_idx], PDE_PRESENT));

  if (!pdp_entry_exists) {
    if (!should_make) {
      return nullptr;
    }
    kmem_set_page_flags(&pdp[pdp_idx], page_creation_flags);
  }

  pml_entry_t* pd = (pml_entry_t*)quick_map((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits));
  const bool pd_entry_exists = pml_entry_is_bit_set(&pd[pd_idx], PDE_PRESENT);

  if (!pd_entry_exists) {
    if (!should_make) {
      return nullptr;
    }
    kmem_set_page_flags(&pd[pd_idx], page_creation_flags);
  }

  // this just should exist
  pml_entry_t* pt = (pml_entry_t*)quick_map((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits));

  return &pt[pt_idx];
}

void kmem_invalidate_pd(uintptr_t vaddr) {
  asm volatile("invlpg %0" ::"m"(vaddr) : "memory");
}

bool kmem_map_page (pml_entry_t* table, uintptr_t virt, uintptr_t phys, uint32_t kmem_flags, uint32_t page_flags) {

  pml_entry_t* page;

  // secure page
  if (page_flags == 0)
    page_flags = KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL;

  page = kmem_get_page(table, virt, kmem_flags, page_flags);
  
  if (!page) {
    return false;
  }

  kmem_set_page_flags(page, page_flags);
  kmem_set_page_base(page, phys);

  /* Let's just unmap, just to be sure */
  try_quickunmap();

  return true;
}

ErrorOrPtr kmem_map_into(pml_entry_t* table, vaddr_t old, vaddr_t new, size_t size, uint32_t kmem_flags, uint32_t page_flags) {

  /* Let's return a default of Error if the caller tries to map a size of zero */
  ErrorOrPtr result = Error();
  size_t page_count;

  if (size == 0)
    goto out;
  
  page_count = kmem_get_page_idx(ALIGN_UP(size, SMALL_PAGE_SIZE));

  for (uintptr_t i = 0; i < page_count; i++) {
    vaddr_t old_vbase = old + (i * SMALL_PAGE_SIZE);
    vaddr_t new_vbase = new + (i * SMALL_PAGE_SIZE);

    paddr_t old_phys = kmem_to_phys_aligned(nullptr, old_vbase);

    if (!kmem_map_page(table, new_vbase, old_phys, kmem_flags, page_flags)) {
      result = Error();
      break;
    }

    result = Success(new);
  } 

out:
  return result;
}

bool kmem_map_range(pml_entry_t* table, uintptr_t virt_base, uintptr_t phys_base, size_t page_count, uint32_t kmem_flags, uint32_t page_flags) {

  for (uintptr_t i = 0; i < page_count; i++) {
    uintptr_t vbase = virt_base + (kmem_get_page_addr(i));
    uintptr_t pbase = phys_base + (kmem_get_page_addr(i));

    if (!kmem_map_page(table, vbase, pbase, kmem_flags, page_flags)) {
      return false;
    }
  }
  return true;
}

bool kmem_unmap_page_ex(pml_entry_t* table, uintptr_t virt, uint32_t custom_flags) {

  pml_entry_t *page;

  page = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page(table, virt, custom_flags, 0));

  if (page) {
    page->raw_bits = 0;
    return true;
  }

  // kmem_invalidate_pd(virt);
  return false;
}

// FIXME: free higher level pts as well
bool kmem_unmap_page(pml_entry_t* table, uintptr_t virt) {
  return kmem_unmap_page_ex(table, virt, 0);
}

bool kmem_unmap_range(pml_entry_t* table, uintptr_t virt, size_t page_count) {

  for (uintptr_t i = 0; i < page_count; i++) {

    vaddr_t virtual_offset = virt + (i * SMALL_PAGE_SIZE);

    if (!kmem_unmap_page(table, virtual_offset)) {
      return false;
    }
  }

  return true;
}

void kmem_set_page_flags(pml_entry_t *page, unsigned int flags) {

  uintptr_t base = kmem_request_physical_page().m_ptr;
  kmem_set_page_base(page, base);

  pml_entry_set_bit(page, PTE_PRESENT, true);
  pml_entry_set_bit(page, PTE_USER, ((flags & KMEM_FLAG_KERNEL) == KMEM_FLAG_KERNEL) ? false : true);
  pml_entry_set_bit(page, PTE_READ_WRITE, ((flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE) ? true : false);
  pml_entry_set_bit(page, PTE_NO_CACHE, ((flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE) ? true : false);
  pml_entry_set_bit(page, PTE_WRITE_THROUGH, ((flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH) ? true : false);
  pml_entry_set_bit(page, PTE_PAT, ((flags & KMEM_FLAG_SPEC) == KMEM_FLAG_SPEC) ? true : false);
  pml_entry_set_bit(page, PTE_NX, ((flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE) ? true : false);

}

/* NEW IMPL */
// allocates a region using the physical allocator and then 
// identity maps it
void* kmem_kernel_alloc(paddr_t addr, size_t size, uint32_t flags) {
  return kmem_alloc(nullptr, addr, size, flags);
}

void* kmem_alloc(pml_entry_t* map, paddr_t addr, size_t size, uint32_t flags) {

  if (kmem_is_phys_page_used(kmem_get_page_idx(addr)) && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {

    /* Already mapped high prob */
    if (!map || map == kmem_get_krnl_dir())
      return (void*)kmem_ensure_high_mapping(addr);

    return nullptr;
  }

  const bool should_identity_map = (flags & KMEM_CUSTOMFLAG_IDENTITY)  == KMEM_CUSTOMFLAG_IDENTITY;
  const paddr_t aligned_paddr = ALIGN_DOWN(addr, SMALL_PAGE_SIZE);
  const vaddr_t aligned_vaddr = kmem_ensure_high_mapping(aligned_paddr);
  const size_t pages_needed = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

  // get the vaddress that is mapped high and we want to return
  const void* ret = (void*)addr;
  if (!should_identity_map) {
    ret = (void*)kmem_ensure_high_mapping(addr);
  }

  for (uintptr_t i = 0; i < pages_needed; i++) {
    const uintptr_t page_idx = kmem_get_page_idx(aligned_paddr);
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (was_used && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
      return nullptr;
    }

    /* Map the aligned p and v addresses */
    const paddr_t p_address = aligned_paddr + (i * SMALL_PAGE_SIZE);
    const vaddr_t v_address = aligned_vaddr + (i * SMALL_PAGE_SIZE);

    /* Mark used */
    kmem_set_phys_page_used(page_idx);

    /* And map */
    bool result = kmem_map_page(map, v_address, p_address, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (!result) {
      return nullptr;
    }
  }

  return (void*)ret;
}

ErrorOrPtr kmem_kernel_dealloc(uintptr_t virt_base, size_t size) {
  return kmem_dealloc(nullptr, virt_base, size);
}

ErrorOrPtr kmem_dealloc(pml_entry_t* map, uintptr_t virt_base, size_t size) {

  const size_t pages_needed = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

  for (uintptr_t i = 0; i < pages_needed; i++) {
    // get the virtual address of the current page
    const vaddr_t vaddr = virt_base + (i * SMALL_PAGE_SIZE);
    // get the physical base of that page
    const paddr_t paddr = kmem_to_phys(map, vaddr);
    // get the index of that physical page
    const uintptr_t page_idx = kmem_get_page_idx(paddr);
    // check if this page is actually used
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (!was_used) 
      return Error();
    
    kmem_set_phys_page_free(page_idx);

    kmem_unmap_page(map, vaddr);
  }
  return Success(0);
}

// FIXME: code duplication
// FIXME: check for alignment
void* kmem_kernel_alloc_extended (uintptr_t addr, size_t size, uint32_t flags, uint32_t page_flags) {
  return kmem_alloc_extended(nullptr, addr, size, flags, page_flags);
}

void* kmem_alloc_extended (pml_entry_t* map, uintptr_t addr, size_t size, uint32_t flags, uint32_t page_flags) {
  const size_t pages_needed = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;
  const bool should_identity_map = (flags & KMEM_CUSTOMFLAG_IDENTITY)  == KMEM_CUSTOMFLAG_IDENTITY;
  const vaddr_t virt_base = should_identity_map ? addr : kmem_ensure_high_mapping(addr);

  for (uintptr_t i = 0; i < pages_needed; i++) {
    const uintptr_t page_idx = kmem_get_page_idx(addr);
    const bool was_used = kmem_is_phys_page_used(page_idx);

    const vaddr_t vaddr = virt_base + (i * SMALL_PAGE_SIZE);
    const paddr_t paddr = addr + (i * SMALL_PAGE_SIZE);

    if (was_used && !(flags & KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE)) {
      return nullptr;
    }

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(map, vaddr, paddr, KMEM_CUSTOMFLAG_GET_MAKE, page_flags);

    if (!result) {
      return nullptr;
    }
  }

  return (void*)virt_base;
}

// @params
// size: size in bytes that should be allocated
// TODO: test
ErrorOrPtr kmem_kernel_alloc_range (size_t size, uint32_t custom_flags, uint32_t page_flags) {
  // Should we use PHYSICAL_MAP_BASE here?
  return kmem_kernel_map_and_alloc_range(size, HIGH_MAP_BASE, custom_flags, page_flags);
}

ErrorOrPtr kmem_kernel_map_and_alloc_range(size_t size, vaddr_t virtual_base, uint32_t custom_flags, uint32_t page_flags) {
  return kmem_map_and_alloc_range(nullptr, size, virtual_base, custom_flags, page_flags);
}

ErrorOrPtr kmem_map_and_alloc_range(pml_entry_t* map, size_t size, vaddr_t virtual_base, uint32_t custom_flags, uint32_t page_flags) {
  const bool should_identity_map = ((custom_flags & KMEM_CUSTOMFLAG_IDENTITY) == KMEM_CUSTOMFLAG_IDENTITY);
  const bool should_remap = ((custom_flags & KMEM_CUSTOMFLAG_NO_REMAP) != KMEM_CUSTOMFLAG_NO_REMAP);

  const size_t pages_needed = kmem_get_page_idx(ALIGN_UP(size, SMALL_PAGE_SIZE));

  const uintptr_t start_idx = Must(bitmap_find_free_range(KMEM_DATA.m_phys_bitmap, pages_needed));
  const paddr_t phys_base = kmem_get_page_addr(start_idx);
  const vaddr_t ret = should_identity_map ? phys_base : (should_remap ? kmem_from_phys(phys_base, virtual_base) : virtual_base); 

  for (uintptr_t i = 0; i < pages_needed; i++) {
    const uintptr_t page_idx = start_idx + i;
    const vaddr_t vaddr = ret + (i * SMALL_PAGE_SIZE);
    const paddr_t paddr = phys_base + (i * SMALL_PAGE_SIZE);

    ASSERT_MSG(!bitmap_isset(KMEM_DATA.m_phys_bitmap, page_idx), "Page index does seem to be used!");

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(map, vaddr, paddr, KMEM_CUSTOMFLAG_GET_MAKE | custom_flags, page_flags);

    if (!result) {
      return Error();
    }
  }

  if ((custom_flags & KMEM_CUSTOMFLAG_GIVE_PHYS) == KMEM_CUSTOMFLAG_GIVE_PHYS) {
    return Success(phys_base);
  }

  return Success(ret);
}

ErrorOrPtr kmem_map_and_alloc_scattered(pml_entry_t* map, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags) {
  const bool should_remap = (!(custom_flags & KMEM_CUSTOMFLAG_NO_REMAP));

  const size_t pages_needed = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

  const vaddr_t ret = vbase; 

  for (uintptr_t i = 0; i < pages_needed; i++) {

    const uintptr_t page_idx = Must(bitmap_find_free(KMEM_DATA.m_phys_bitmap));
    const paddr_t page_base = kmem_get_page_addr(page_idx);

    const vaddr_t vaddr = vbase + (i * SMALL_PAGE_SIZE);
    const paddr_t paddr = page_base;

    kmem_set_phys_page_used(page_idx);
    bool result = kmem_map_page(map, vaddr, paddr, custom_flags, page_flags);

    if (!result) {
      return Error();
    }
  }
  return Success(ret);

}

// TODO: make this more dynamic
static inline void _init_kmem_page_layout () {

  //kmem_map_range(nullptr, 0, 0, KMEM_DATA.m_phys_pages_count, KMEM_CUSTOMFLAG_GET_MAKE, 0);

  // Map all the free ranges to their High mappings
  // FIXME: when dealing with systems that have a lot of memory,
  // we throw a pagefault here, since we try to create tables 
  // in memoryregions that are not yet mapped...
  // NOTE: this really seems to die on larges systems,
  // but I don't want to map a standard of 4 Gigs.
  // that just seems excessive
  /*
  FOREACH(i, KMEM_DATA.m_contiguous_ranges) {
    contiguous_phys_virt_range_t* range = i->data;
    const vaddr_t vbase = kmem_ensure_high_mapping(range->lower);
    const paddr_t pbase = range->lower;
    const size_t size = range->upper - range->lower;
    // size / SMALL_PAGE_SIZE
    size_t pages = size >> 12;

    kmem_map_range(nullptr, vbase, pbase, pages, KMEM_CUSTOMFLAG_GET_MAKE, 0);
  }
  */

  const paddr_t kernel_physical_end = ALIGN_UP((uintptr_t)&_kernel_end - HIGH_MAP_BASE, SMALL_PAGE_SIZE);
  const size_t total_kernel_page_count = kmem_get_page_idx(kernel_physical_end);
  /* Map some extra Megabytes in order to ensure that we can map more later */
  const size_t extra_mappings = kmem_get_page_idx(ALIGN_UP(10 * Mib, SMALL_PAGE_SIZE));

  kmem_map_range(nullptr, HIGH_MAP_BASE, 0, total_kernel_page_count + extra_mappings, KMEM_CUSTOMFLAG_GET_MAKE, 0);

  init_quickmapper();

  println("Done mapping ranges");
}

page_dir_t kmem_create_page_dir(uint32_t custom_flags, size_t initial_mapping_size) {

  println_kterm("Creating page dir");
  page_dir_t ret = { 0 };

  const bool create_user = ((custom_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER);
  /* Make sure we never start with 0 pages */
  const size_t page_count = initial_mapping_size ? ((ALIGN_UP(initial_mapping_size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE) + 1) : 0;

  /* 
   * We can't just take a clean page here, since we need it mapped somewhere... 
   * For that we'll have to use the kernel allocation feature to instantly map
   * it somewhere for us in kernelspace =D
   */
  pml_entry_t* table_root = (pml_entry_t*)Must(kmem_kernel_alloc_range(SMALL_PAGE_SIZE, 0, 0));
  pml_entry_t* phys_table_root = (void*)kmem_to_phys(nullptr, (uintptr_t)table_root);
  println_kterm("Created root");

  for (uintptr_t i = 0; i < page_count; i++) {

    paddr_t physical_base = Must(kmem_prepare_new_physical_page());
    vaddr_t virtual_base = i * SMALL_PAGE_SIZE;

    bool result = kmem_map_page(phys_table_root, virtual_base, physical_base, custom_flags, KMEM_FLAG_WRITABLE | (create_user ? 0 : KMEM_FLAG_KERNEL));

    if (!result) {
      return ret;
    }
  }

  const paddr_t kernel_physical_end = ALIGN_UP((uintptr_t)&_kernel_end - HIGH_MAP_BASE, SMALL_PAGE_SIZE);
  const size_t total_kernel_page_count = kmem_get_page_idx(kernel_physical_end);

  println_kterm("Mapping kernel into: ");
  println_kterm(to_string((uintptr_t)phys_table_root));
  println_kterm(to_string(kernel_physical_end));
  println_kterm(to_string(total_kernel_page_count));

  Must(kmem_copy_kernel_mapping(table_root));

  //kmem_map_range(phys_table_root, HIGH_MAP_BASE, 0, total_kernel_page_count, KMEM_CUSTOMFLAG_GET_MAKE, 0);

  ret.m_root = table_root;
  ret.m_kernel_low = HIGH_MAP_BASE;
  ret.m_kernel_high = HIGH_MAP_BASE + (kernel_physical_end >> 12);

  println_kterm("Created page dir");
  return ret;

error_and_out:
  kernel_panic("Failed to create page dir! (TODO: implement resolve)");
}

void kmem_copy_bytes_into_map(vaddr_t vbase, void* buffer, size_t size, pml_entry_t* map) {

  // paddr_t entry = kmem_to_phys(map, vbase);

  /* Mapping bytes into a virtial address of a different pmap */

  /*
   * TODO:  step 1 -> find physical address 
   *        step 2 -> map into current map
   *        step 3 -> copy bytes
   *        step 4 -> unmap in current map
   *        step 5 -> move over to the next page
   */

  kernel_panic("TODO: implement kmem_copy_bytes_into_map");
}

ErrorOrPtr kmem_to_current_pagemap(vaddr_t vaddr, pml_entry_t* external_map) {
  Processor_t* current = get_current_processor();

  pml_entry_t* current_map = current->m_page_dir;

  if (!current_map)
    current_map = kmem_get_krnl_dir();

  paddr_t phys = kmem_to_phys(external_map, vaddr);

  kernel_panic("TODO: implement kmem_to_current_pagemap");

  /* TODO: */
  return Error();
}

/*
 * TODO: do we want to murder an entire addressspace, or do we just want to get rid 
 * of the mappings?
 */
void kmem_destroy_page_dir(pml_entry_t* dir) {
  kernel_panic("TODO: implement kmem_destroy_page_dir");
}

/*
 * NOTE: caller needs to ensure that they pass a physical address
 * as page map. CR3 only takes physical addresses
 */
void load_page_dir(uintptr_t dir, bool __disable_interupts) {
  if (__disable_interupts) 
    disable_interrupts();

  pml_entry_t* page_map = (pml_entry_t*)dir;

  ASSERT(get_current_processor() != nullptr);
  get_current_processor()->m_page_dir = page_map;

  /*
  proc_t* current_proc = get_current_proc();

  if (current_proc) {
    current_proc->m_root_pd.m_root = (pml_entry_t*)dir;
    current_proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
    current_proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
  }
  */

  asm volatile("" : : : "memory");
  asm volatile("movq %0, %%cr3" ::"r"(dir));
  asm volatile("" : : : "memory");
}

ErrorOrPtr kmem_copy_kernel_mapping(pml_entry_t* new_table) {

  const vaddr_t base = HIGH_MAP_BASE;
  const uintptr_t pml4_idx = (base >> 39) & ENTRY_MASK;

  pml_entry_t* kernel_root = (void*)kmem_ensure_high_mapping((uintptr_t)kmem_get_krnl_dir());

  pml_entry_t kernel_lvl_3 = kernel_root[pml4_idx];
  bool is_present = pml_entry_is_bit_set(&kernel_lvl_3, PDE_PRESENT);

  if (!is_present)
    return Error();

  new_table[pml4_idx] = kernel_lvl_3;

  return Success(0);
}

// FIXME: macroes?
uintptr_t kmem_get_page_idx (uintptr_t page_addr) {
  return (ALIGN_DOWN(page_addr, SMALL_PAGE_SIZE) >> 12);
}
uintptr_t kmem_get_page_addr (uintptr_t page_idx) {
  return (page_idx << 12);
}

uintptr_t kmem_get_page_base (uintptr_t base) {
  return (base & ~(0xffful));
}

uintptr_t kmem_set_page_base(pml_entry_t* entry, uintptr_t page_base) {
  entry->raw_bits &= 0x8000000000000fffULL;
  entry->raw_bits |= kmem_get_page_base(page_base);
  return entry->raw_bits;
}

// deprecated
pml_entry_t create_pagetable(uintptr_t paddr, bool writable) {
  pml_entry_t table = {
    .raw_bits = paddr
  };

  pml_entry_set_bit(&table, PDE_PRESENT, true);
  pml_entry_set_bit(&table, PDE_READ_WRITE, writable);

  return table;
}

list_t const* kmem_get_phys_ranges_list() {
  return KMEM_DATA.m_phys_ranges;
}

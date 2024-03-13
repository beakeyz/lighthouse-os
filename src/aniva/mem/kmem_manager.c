#include "kmem_manager.h"
#include "entry/entry.h"
#include <mem/heap.h>
#include "logging/log.h"
#include "mem/page_dir.h"
#include "mem/pg.h"
#include "libk/multiboot.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "system/processor/processor.h"
#include "system/resource.h"
#include <mem/heap.h>
#include <libk/stddef.h>
#include <libk/string.h>
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/manifest.h>

/*
 * Aniva kernel memory manager
 *
 * This file is a big mess. There is unused code, long and unreadable code, stuff that's half broken and
 * stuff that just really likes to break for some reason. This file needs a big dust-off ASAP
 */

#define STANDARD_PD_ENTRIES 512

/*
 * Why is this a struct?
 *
 * Well, I wanted to have a more modulare kmem system, but it turned out to be 
 * static as all hell. I've kept this so far since it might come in handy when I want 
 * to migrate this code to support SMP eventualy. Then there should be multiple kmem managers
 * running on more than one CPU which makes it nice to have a struct per manager
 * to keep track of their state. 
 */
static struct {
  uint32_t m_mmap_entry_num;
  uint32_t m_kmem_flags;

  multiboot_memory_map_t* m_mmap_entries;

  bitmap_t* m_phys_bitmap;
  uintptr_t m_highest_phys_addr;
  size_t m_phys_pages_count;
  size_t m_total_avail_memory_bytes;
  size_t m_total_unavail_memory_bytes;

  /*
   * This is the base that we use for high mappings 
   * Before we have done the late init this is HIGH_MAP_BASE
   * After we have done the late init this is KERNEL_MAP_BASE
   * Both of these virtual bases map to phys 0 which makes them
   * easy to work with in terms of transforming phisical to 
   * virtual addresses, but the only difference is that KERNEL_MAP_BASE
   * has more range
   */
  vaddr_t m_high_page_base;

  list_t* m_phys_ranges;

  pml_entry_t* m_kernel_base_pd;
} KMEM_DATA;

/* Variable from boot.asm */
extern const size_t early_map_size;

static void _init_kmem_page_layout();
static void kmem_init_physical_allocator();

// first prep the mmap
void prep_mmap(struct multiboot_tag_mmap *mmap) 
{
  KMEM_DATA.m_mmap_entry_num = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
  KMEM_DATA.m_mmap_entries = (struct multiboot_mmap_entry *)mmap->entries;
}

/*
 * TODO: Implement a lock around the physical allocator
 * FIXME: this could be heapless
 */
void init_kmem_manager(uintptr_t* mb_addr) 
{
  KMEM_DATA.m_phys_ranges = init_list();
  KMEM_DATA.m_kmem_flags = 0;

  // nested fun
  prep_mmap(get_mb2_tag((void *)mb_addr, 6));
  parse_mmap();

  kmem_init_physical_allocator();

  // Perform multiboot finalization
  finalize_multiboot();

  _init_kmem_page_layout();

  KMEM_DATA.m_kmem_flags |= KMEM_STATUS_FLAG_DONE_INIT;
}

void debug_kmem()
{
  uint32_t idx = 0;

  FOREACH(i, KMEM_DATA.m_phys_ranges) {
    phys_mem_range_t* range = i->data;

    printf("Memory range (%d) starting at 0x%llx with 0x%llx bytes and type=%d\n", idx, range->start, range->length, range->type);

    idx++;
  }

  printf("Got phys bitmap: 0x%llx (%lld bytes)\n", (uint64_t)KMEM_DATA.m_phys_bitmap & ~(HIGH_MAP_BASE), KMEM_DATA.m_phys_bitmap->m_size);
}

int kmem_get_info(kmem_info_t* info_buffer, uint32_t cpu_id)
{
  uint64_t total_used_pages = 0;

  if (!info_buffer)
    return -1;
  
  for (uint64_t i = 0; i < KMEM_DATA.m_phys_bitmap->m_entries; i++) {
    if (bitmap_isset(KMEM_DATA.m_phys_bitmap, i))
      total_used_pages++;
  }

  memset(info_buffer, 0, sizeof(*info_buffer));

  info_buffer->cpu_id = cpu_id;
  info_buffer->flags = KMEM_DATA.m_kmem_flags;
  info_buffer->memsize = KMEM_DATA.m_total_avail_memory_bytes;
  info_buffer->free_pages = KMEM_DATA.m_phys_pages_count - total_used_pages;
  info_buffer->used_pages = total_used_pages;

  return 0;
}

/*!
 * @brief: Try to carve out @targetrange out from @range
 */
static void _carve_out_range(phys_mem_range_t* range, contiguous_phys_virt_range_t* targetrange)
{
  phys_mem_range_t* new_range;
  uint64_t p_range_end = ALIGN_DOWN(range->start + range->length, SMALL_PAGE_SIZE);
  uint64_t p_targetrange_end = ALIGN_DOWN(targetrange->end, SMALL_PAGE_SIZE);

  /* The target range is entirely above @range */
  if (targetrange->start > p_range_end)
    return;

  /* The target range is entirely below @range */
  if (range->start > p_targetrange_end)
    return;

  /* Parse out the target range */
  if (range->start <= targetrange->start && p_range_end >= targetrange->end) {
    /* target is completely placed somewhere inside this range */
    new_range = kmalloc(sizeof(*new_range));
    new_range->start = ALIGN_DOWN(targetrange->start, SMALL_PAGE_SIZE);
    new_range->length = ALIGN_UP(targetrange->end - targetrange->start, SMALL_PAGE_SIZE);
    new_range->type = PMRT_KERNEL_RESERVED;

    list_append(KMEM_DATA.m_phys_ranges, new_range);

    /* Shorten the range that's currently here */
    range->length = ALIGN_UP(targetrange->start - range->start, SMALL_PAGE_SIZE);

    /* Append another range to fix the higher part of the range we are fucking with */
    new_range = kmalloc(sizeof(*new_range));
    new_range->start = ALIGN_UP(targetrange->end, SMALL_PAGE_SIZE);
    new_range->length = ALIGN_UP(p_range_end - targetrange->end, SMALL_PAGE_SIZE);
    new_range->type = PMRT_USABLE;

    /* Only append if there is room still above the kernel in this range */
    if (!new_range->length)
      kfree(new_range);
    else
      list_append(KMEM_DATA.m_phys_ranges, new_range);

  } else if (range->start <= targetrange->start && p_range_end > targetrange->start) {
    /* Only the first part of the kernel is inside this range */
    new_range = kmalloc(sizeof(*new_range));
    new_range->start = targetrange->start;
    new_range->length = p_range_end - targetrange->start;
    new_range->type = PMRT_KERNEL_RESERVED;

    list_append(KMEM_DATA.m_phys_ranges, new_range);

    /* Only length changes in this case */
    range->length -= new_range->length;
  } else if (p_range_end >= targetrange->end && range->start < targetrange->end) {
    /* Only the last part of the kernel is inside this range */
    new_range = kmalloc(sizeof(*new_range));
    new_range->start = range->start;
    new_range->length = targetrange->end - range->start;
    new_range->type = PMRT_KERNEL_RESERVED;

    list_append(KMEM_DATA.m_phys_ranges, new_range);

    range->start += new_range->length;
    range->length -= new_range->length;
  } else if (range->start > targetrange->start && p_range_end < targetrange->end) {
    /* (For some reason) This range is completely inside the kernel (WTF?) */
    range->type = PMRT_KERNEL_RESERVED;
  }
}

// function inspired by serenityOS
void parse_mmap() 
{
  paddr_t _p_kernel_start;
  paddr_t _p_kernel_end;
  uintptr_t p_range_end;

  KMEM_DATA.m_phys_pages_count = 0;
  KMEM_DATA.m_highest_phys_addr = 0;

  _p_kernel_start = (uintptr_t)&_kernel_start & ~HIGH_MAP_BASE;
  _p_kernel_end = ((uintptr_t)&_kernel_end & ~HIGH_MAP_BASE) + g_system_info.post_kernel_reserved_size;

  /*
   * These are the static memory regions that we need to reserve in order for the system to boot
   */
  contiguous_phys_virt_range_t reserved_ranges[] = {
    { _p_kernel_start, _p_kernel_end },
  };

  for (uintptr_t i = 0; i < KMEM_DATA.m_mmap_entry_num; i++) {
    multiboot_memory_map_t *map = &KMEM_DATA.m_mmap_entries[i];

    // TODO: register physical memory ranges
    phys_mem_range_t* range = kmalloc(sizeof(phys_mem_range_t));
    range->start = map->addr;
    range->length = map->len;

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

    /* DEBUG */
    print("Type: ");
    println(to_string(range->type));
    print("map entry start addr: ");
    println(to_string(range->start));
    print("map entry length: ");
    println(to_string(range->length));
    
    if (range->type != PMRT_USABLE)
      continue;

    range->start = ALIGN_DOWN(range->start, SMALL_PAGE_SIZE);
    range->length = ALIGN_UP(range->length, SMALL_PAGE_SIZE);

    p_range_end = ALIGN_DOWN(range->start + range->length, SMALL_PAGE_SIZE);

    if (p_range_end > KMEM_DATA.m_highest_phys_addr)
      KMEM_DATA.m_highest_phys_addr = p_range_end;

    printf("Adding %lld bytes to %lld\n", range->length, KMEM_DATA.m_total_avail_memory_bytes);
    KMEM_DATA.m_total_avail_memory_bytes += range->length;
    printf("We now have %lld\n", KMEM_DATA.m_total_avail_memory_bytes);

    for (uint32_t i = 0; i < arrlen(reserved_ranges); i++)
      _carve_out_range(range, &reserved_ranges[i]);

  }

  KMEM_DATA.m_phys_pages_count = GET_PAGECOUNT(0, KMEM_DATA.m_total_avail_memory_bytes);

  print("Total contiguous pages: ");
  println(to_string(KMEM_DATA.m_phys_pages_count));
}

/*!
 * @brief: Finds a free physical range that fits @size bytes
 *
 * When a usable range is found, we truncate the range in the physical range list and return a
 * dummy range. This range won't be included in the total physical range list, but that won't be
 * needed, as we will only look at ranges marked as usable. This means we can get away with only 
 * resizing the range we steal bytes from
 */
static int _allocate_free_physical_range(phys_mem_range_t* range, size_t size)
{
  size_t mapped_size;
  phys_mem_range_t* c_range;
  phys_mem_range_t* selected_range;

  mapped_size = NULL;
  selected_range = NULL;

  /* Make sure we're allocating on page boundries */
  size = ALIGN_UP(size + SMALL_PAGE_SIZE, SMALL_PAGE_SIZE);

  printf("Looking for %lld bytes\n", size);
  printf("We already have %lld bytes mapped\n", early_map_size);

  FOREACH(i, KMEM_DATA.m_phys_ranges) {
    c_range = i->data;

    /* Only look for usable ranges */
    if (c_range->type != PMRT_USABLE)
      continue;

    /* Can't use any ranges that aren't mapped */
    if (c_range->start >= early_map_size)
      continue;

    if (c_range->length < size)
      continue;

    /* Assume we're mapped */
    mapped_size = c_range->length;

    /* If the range is only partially mapped, we need to figure out if we can fit into the mapped part */
    if ((c_range->start + c_range->length) > early_map_size)
      mapped_size = early_map_size - c_range->start;

    /* Get the stuff we need */
    selected_range = c_range;

    /* Check if we fit */
    if (size < mapped_size)
      break;

    /* Fuck, reset and go next */
    selected_range = NULL;
  }

  if (!selected_range)
    return -1;

  /* Create a new dummy range */
  range->start = ALIGN_DOWN(selected_range->start, SMALL_PAGE_SIZE);
  range->length = size;
  range->type = PMRT_RESERVED;

  selected_range->start += size;
  selected_range->length -= size;

  return 0;
}

/*
 * Initialize the physical page allocator that keeps track
 * of which physical pages are used and which are free
 */
static void kmem_init_physical_allocator() 
{
  phys_mem_range_t bm_range;
  bitmap_t* physical_bitmap;
  vaddr_t bitmap_start_addr;
  size_t physical_bitmap_size;
  size_t physical_pages_bytes;

  physical_pages_bytes = (ALIGN_UP(KMEM_DATA.m_phys_pages_count, 8) >> 3);
  physical_bitmap_size = physical_pages_bytes + sizeof(bitmap_t);

  /* Find a bitmap. This is crucial */
  ASSERT_MSG(_allocate_free_physical_range(&bm_range, physical_bitmap_size) == NULL, "Failed to find a physical range for our physmem bitmap!");

  /* Compute where our bitmap MUST go */
  bitmap_start_addr = kmem_ensure_high_mapping(bm_range.start);

  /*
   * new FIXME: we allocate a bitmap for the entire memory page space on the heap, 
   * which means that on systems with a lot of RAM, we need a biiig heap to contain 
   * that. That's an issue, because we need to reserve that space right now in the form of 
   * a static memory space that gets embedded into the kernel binary, because in order
   * for us to have a dynamic heap like this, we'll need a working physical allocator, 
   * but for that we also need a dynamic heap... 
   * The solution would be to map enough memory in the boot stub to try to find a suitable 
   * location for the bitmap to fit manualy and then initialize the heap after that.
   */
  physical_bitmap = (bitmap_t*)(bitmap_start_addr);

  physical_bitmap->m_size = physical_pages_bytes;
  physical_bitmap->m_entries = physical_pages_bytes * 8;
  physical_bitmap->m_default = 0xff;

  printf("Trying to initialize our bitmap at 0x%llx with %lld entries\n", bitmap_start_addr, physical_bitmap->m_entries);

  memset(physical_bitmap->m_map, physical_bitmap->m_default, physical_pages_bytes);

  KMEM_DATA.m_phys_bitmap = physical_bitmap;

  paddr_t base;
  size_t size;

  // Mark the contiguous 'free' ranges as free in our bitmap
  FOREACH(i, KMEM_DATA.m_phys_ranges) {
    const phys_mem_range_t* range = i->data;

    if (range->type != PMRT_USABLE)
      continue;

    base = range->start;
    size = range->length;

    /* Base and size should already be page-aligned */
    kmem_set_phys_range_free(
        kmem_get_page_idx(base),
        GET_PAGECOUNT(base, size));
  }
  
  /* Make sure this is safe */
  kmem_set_phys_range_used(
      kmem_get_page_idx(bm_range.start),
      GET_PAGECOUNT(bm_range.start, bm_range.length));

  /* Reserve the bottom megabyte of physical memory */
  const size_t low_reserve = 64 * Kib;

  kmem_set_phys_range_used(0, GET_PAGECOUNT(0, low_reserve));
}

/*
 * Transform a physical address to an arbitrary virtual base
 */
vaddr_t kmem_from_phys(uintptr_t addr, vaddr_t vbase) 
{
  return (vaddr_t)(vbase | addr);
}

vaddr_t kmem_from_dma(paddr_t addr)
{
  return kmem_from_phys(addr, IO_MAP_BASE);
}

/*
 * Transform a physical address to an address that is mapped to the 
 * high kernel range
 */
vaddr_t kmem_ensure_high_mapping(uintptr_t addr) 
{
  return kmem_from_phys(addr, HIGH_MAP_BASE);
}


/*
 * Translate a virtual address to a physical address that is
 * page-aligned
 */
uintptr_t kmem_to_phys_aligned(pml_entry_t* root, uintptr_t addr) 
{
  pml_entry_t *page = kmem_get_page(root, ALIGN_DOWN(addr, SMALL_PAGE_SIZE), 0, 0);

  if (page == nullptr)
    return NULL;

  return kmem_get_page_base(page->raw_bits);
}

/*
 * Same as the function above, but this one also keeps the alignment in mind
 */
uintptr_t kmem_to_phys(pml_entry_t *root, uintptr_t addr) {

  vaddr_t aligned_vaddr = ALIGN_DOWN(addr, SMALL_PAGE_SIZE);
  size_t delta = addr - aligned_vaddr;

  pml_entry_t *page = kmem_get_page(root, addr, 0, 0);

  /* Address is not mapped */
  if (page == nullptr)
    return NULL;

  paddr_t aligned_base = kmem_get_page_base(page->raw_bits);

  return aligned_base + delta;
}

// flip a bit to 1 as to mark a pageframe as used in our bitmap
void kmem_set_phys_page_used(uintptr_t idx) {
  bitmap_mark(KMEM_DATA.m_phys_bitmap, idx);
}

// flip a bit to 0 as to mark a pageframe as free in our bitmap
void kmem_set_phys_page_free(uintptr_t idx) {
  //printf("kmem_set_phys_page_free %lld\n", idx);
  bitmap_unmark(KMEM_DATA.m_phys_bitmap, idx);
}

void kmem_set_phys_range_used(uintptr_t start_idx, size_t page_count) {
  for (uintptr_t i = 0; i < page_count; i++) {
    kmem_set_phys_page_used(start_idx + i);
  }
}

void kmem_set_phys_range_free(uintptr_t start_idx, size_t page_count) {
  for (uintptr_t i = 0; i < page_count; i++) {
    kmem_set_phys_page_free(start_idx + i);
  }
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

/*
 * Try to find a free physical page, using the bitmap that was 
 * setup by the physical page allocator
 */
ErrorOrPtr kmem_request_physical_page() {

  TRY(index, bitmap_find_free(KMEM_DATA.m_phys_bitmap));

  return Success(index);
}

/*
 * Try to find a free physical page, just as kmem_request_physical_page, 
 * but this also marks it as used and makes sure it is filled with zeros
 *
 * FIXME: we might grab a page that is located at kernel_end + 1 Gib which won't be 
 * mapped to our kernel HIGH map extention. When this happens, we want to have
 */
ErrorOrPtr kmem_prepare_new_physical_page() 
{
  paddr_t address;
  paddr_t p_page_idx;
  ErrorOrPtr res;

  // find
  res = kmem_request_physical_page();

  /* Fuck */
  if (IsError(res))
    return res;

  p_page_idx = Release(res);

  kmem_set_phys_page_used(p_page_idx);

  address = kmem_get_page_addr(p_page_idx);

  // There might be an issue here as we try to zero
  // and this page is never mapped. We might need some
  // sort of temporary mapping like serenity has to do
  // this, but we might also rewrite this entire thing
  // as to map this sucker before we zero. This would 
  // mean that you can't just get a nice clean page...
  //memset((void*)kmem_from_phys(address, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

  return Success(address);
}

/*
 * Return a physical page to the physical page allocator
 */
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

/*
 * Find the kernel root page directory
 */
pml_entry_t *kmem_get_krnl_dir() {
  /* NOTE: this is a physical address :clown: */
  return KMEM_DATA.m_kernel_base_pd;
}

/*
 * This function is to be used after the bootstrap pagemap has been 
 * discarded. When we create a new mapping, we pull a page from a 
 * pool of virtual memory that is only used for that purpose and sustains itself
 * by mapping the pages it uses for the creation of the pagetable into itself.
 * 
 * TODO: implement this and also create easy virtual pool allocation, creation, deletion,
 * ect. I want to be able to easily see which virtual memory is used per pagemap and
 * we need a good idea of where we map certain resources and things (i.e. drivers, I/O ranges,
 * generic kernel_resources, ect.)
 */
pml_entry_t *kmem_get_page(pml_entry_t* root, vaddr_t addr, unsigned int kmem_flags, uint32_t page_flags) 
{
  /* Make sure the virtual address is not fucking us in our ass */
  addr = addr & PDE_SIZE_MASK;

  const uintptr_t pml4_idx = (addr >> 39) & ENTRY_MASK;
  const uintptr_t pdp_idx = (addr >> 30) & ENTRY_MASK;
  const uintptr_t pd_idx = (addr >> 21) & ENTRY_MASK;
  const uintptr_t pt_idx = (addr >> 12) & ENTRY_MASK;
  const bool should_make = ((kmem_flags & KMEM_CUSTOMFLAG_GET_MAKE) == KMEM_CUSTOMFLAG_GET_MAKE);
  const bool should_make_user = ((kmem_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER);
  const uint32_t page_creation_flags = (should_make_user ? 0 : KMEM_FLAG_KERNEL) | page_flags;

  pml_entry_t* pml4 = (pml_entry_t*)kmem_from_phys((uintptr_t)(root == nullptr ? kmem_get_krnl_dir() : root), KMEM_DATA.m_high_page_base);
  const bool pml4_entry_exists = (pml_entry_is_bit_set(&pml4[pml4_idx], PDE_PRESENT));

  if (!pml4_entry_exists) {
    if (!should_make) {
      return nullptr;
    }

    uintptr_t page_addr = Must(kmem_prepare_new_physical_page());

    memset((void*)kmem_from_phys(page_addr, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

    kmem_set_page_base(&pml4[pml4_idx], page_addr);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_PRESENT, true);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_PAT, true);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_WRITABLE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
    pml_entry_set_bit(&pml4[pml4_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);
  }

  pml_entry_t* pdp = (pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits), KMEM_DATA.m_high_page_base);
  const bool pdp_entry_exists = (pml_entry_is_bit_set((pml_entry_t*)&pdp[pdp_idx], PDE_PRESENT));

  if (!pdp_entry_exists) {
    if (!should_make) {
      return nullptr;
    }

    uintptr_t page_addr = Must(kmem_prepare_new_physical_page());

    memset((void*)kmem_from_phys(page_addr, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

    kmem_set_page_base(&pdp[pdp_idx], page_addr);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_PRESENT, true);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_PAT, true);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_WRITABLE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
    pml_entry_set_bit(&pdp[pdp_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);
  }

  pml_entry_t* pd = (pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits), KMEM_DATA.m_high_page_base);
  const bool pd_entry_exists = pml_entry_is_bit_set(&pd[pd_idx], PDE_PRESENT);

  if (!pd_entry_exists) {
    if (!should_make) {
      return nullptr;
    }

    uintptr_t page_addr = Must(kmem_prepare_new_physical_page());

    memset((void*)kmem_from_phys(page_addr, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

    kmem_set_page_base(&pd[pd_idx], page_addr);
    pml_entry_set_bit(&pd[pd_idx], PDE_PRESENT, true);
    pml_entry_set_bit(&pd[pd_idx], PDE_PAT, true);
    pml_entry_set_bit(&pd[pd_idx], PDE_USER, (page_creation_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
    pml_entry_set_bit(&pd[pd_idx], PDE_WRITABLE, (page_creation_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
    pml_entry_set_bit(&pd[pd_idx], PDE_WRITE_THROUGH, (page_creation_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
    pml_entry_set_bit(&pd[pd_idx], PDE_GLOBAL, (page_creation_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
    pml_entry_set_bit(&pd[pd_idx], PDE_NO_CACHE, (page_creation_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);
    pml_entry_set_bit(&pd[pd_idx], PDE_NX, (page_creation_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);

  }

  // this just should exist
  const pml_entry_t* pt = (const pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits), KMEM_DATA.m_high_page_base);
  return (pml_entry_t*)&pt[pt_idx];
}

/*
 * NOTE: table needs to be a physical pointer
 */
pml_entry_t* kmem_get_page_with_quickmap (pml_entry_t* table, vaddr_t virt, uint32_t kmem_flags, uint32_t page_flags) {
  kernel_panic("kmem_get_page_with_quickmap: not implemented");
}

void kmem_invalidate_tlb_cache_entry(uintptr_t vaddr) {
  asm volatile("invlpg (%0)" : : "r"(vaddr));

  /* TODO: send message to other cores that they need to 
  perform a tlb shootdown aswell on this address */
}

void kmem_invalidate_tlb_cache_range(uintptr_t vaddr, size_t size) {

  vaddr_t virtual_base = ALIGN_DOWN(vaddr, SMALL_PAGE_SIZE);
  uintptr_t indices = kmem_get_page_idx(size);

  for (uintptr_t i = 0; i < indices; i++) {
    kmem_invalidate_tlb_cache_entry(virtual_base + i * SMALL_PAGE_SIZE);
  }
}

void kmem_refresh_tlb() {
  /* FIXME: is this always up to date? */
  pml_entry_t* current = get_current_processor()->m_page_dir;

  /* Reloading CR3 will clear the tlb */
  kmem_load_page_dir((uintptr_t)current, false);
}

bool kmem_map_page (pml_entry_t* table, vaddr_t virt, paddr_t phys, uint32_t kmem_flags, uint32_t page_flags) 
{
  pml_entry_t* page;
  uintptr_t phys_idx;
  bool should_mark;
  bool was_used;

  /* 
   * secure page 
   * NOTE: we don't use KMEM_FLAG_KERNEL here
   * since otherwise it is impossible to specify 
   * userpages like this =/ semantics are weird here lmao
   * TODO: remove this weird behaviour, we should promote verbosity
   */
  if (page_flags == 0 && !(kmem_flags & KMEM_CUSTOMFLAG_READONLY))
    page_flags = KMEM_FLAG_WRITABLE;

  phys_idx = kmem_get_page_idx(phys);

  /* Does the caller specify we should not mark? */
  should_mark = ((kmem_flags & KMEM_CUSTOMFLAG_NO_MARK) != KMEM_CUSTOMFLAG_NO_MARK);
  was_used = kmem_is_phys_page_used(phys_idx);
  
  /*
   * Set the physical page as used here to prevent 
   * kmem_get_page from grabbing it 
   */
  if (should_mark)
    kmem_set_phys_page_used(phys_idx);

  page = kmem_get_page(table, virt, kmem_flags, page_flags);
  
  if (!page)
    return false;

  /*
   * FIXME: we are doing something very dangerous here. We don't give a fuck about the
   * current state of this page and we simply overwrite whats currently there with our
   * physical address
   */
  kmem_set_page_base(page, phys);
  kmem_set_page_flags(page, page_flags);

  /* 
   * If the caller does not specify that they want to defer 
   * freeing the physical page, we just do it now
   */
  if (should_mark && !was_used)
    kmem_set_phys_page_free(phys_idx);

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

    if (!old_phys) {
      return Error();
    }

    if (!kmem_map_page(table, new_vbase, old_phys, kmem_flags, page_flags)) {
      result = Error();
      break;
    }

    result = Success(new);
  } 

out:
  return result;
}

bool kmem_map_range(pml_entry_t* table, uintptr_t virt_base, uintptr_t phys_base, size_t page_count, uint32_t kmem_flags, uint32_t page_flags) 
{
  size_t page_shift;
  const bool should_map_2mib = (kmem_flags & KMEM_CUSTOMFLAG_2MIB_MAP) == KMEM_CUSTOMFLAG_2MIB_MAP;

  /* Can't map zero pages =( */
  if (!page_count)
    return false;

  page_shift = should_map_2mib ? LARGE_PAGE_SHIFT : PAGE_SHIFT;

  if (should_map_2mib)
    page_flags |= KMEM_FLAG_SPEC;

  for (uintptr_t i = 0; i < page_count; i++) {
    const uintptr_t offset = i * (1 << page_shift);
    const uintptr_t vbase = virt_base + offset;
    const uintptr_t pbase = phys_base + offset;

    /* Make sure we don't mark in kmem_map_page, since we already pre-mark the range */
    if (!kmem_map_page(table, vbase, pbase, kmem_flags, page_flags))
      return false;
  }

  return true;
}

/* Assumes we have a physical address buffer */
#define DO_PD_CHECK(parent, entry, idx)                    \
 do {                                                      \
   for (uintptr_t i = 0; i < 512; i++) {                   \
     if (pml_entry_is_bit_set(&entry[i], PTE_PRESENT)) {   \
       return false;                                       \
     }                                                     \
   }                                                       \
   parent[idx].raw_bits = NULL;                            \
   phys = kmem_to_phys_aligned(table, (vaddr_t)entry);     \
   kmem_set_phys_page_free(kmem_get_page_idx(phys));       \
 } while (0)

static bool __kmem_do_recursive_unmap(pml_entry_t* table, uintptr_t virt)
{
  /* This part is a small copy of kmem_get_page, but we need our own nuance here, so fuck off will ya? */
  bool entry_exists;
  paddr_t phys;

  /* Indices we need for recursive deallocating */
  const uintptr_t pml4_idx = (virt >> 39) & ENTRY_MASK;
  const uintptr_t pdp_idx = (virt >> 30) & ENTRY_MASK;
  const uintptr_t pd_idx = (virt >> 21) & ENTRY_MASK;

  pml_entry_t* pml4 = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)(table == nullptr ? kmem_get_krnl_dir() : table));
  entry_exists = (pml_entry_is_bit_set(&pml4[pml4_idx], PDE_PRESENT));

  if (!entry_exists)
    return false;

  pml_entry_t* pdp = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits));
  entry_exists = (pml_entry_is_bit_set(&pdp[pdp_idx], PDE_PRESENT));

  if (!entry_exists)
    return false;

  pml_entry_t* pd = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits));
  entry_exists = (pml_entry_is_bit_set(&pd[pd_idx], PDE_PRESENT));

  if (!entry_exists)
    return false;

  pml_entry_t* pt = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits));
  
  DO_PD_CHECK(pd, pt, pd_idx);
  DO_PD_CHECK(pdp, pd, pdp_idx);
  DO_PD_CHECK(pml4, pdp, pml4_idx);

  return true;
}

bool kmem_unmap_page_ex(pml_entry_t* table, uintptr_t virt, uint32_t custom_flags) 
{
  pml_entry_t *page = kmem_get_page(table, virt, custom_flags, 0);

  if (!page)
    return false;

  page->raw_bits = NULL;

  if ((custom_flags & KMEM_CUSTOMFLAG_RECURSIVE_UNMAP) == KMEM_CUSTOMFLAG_RECURSIVE_UNMAP)
    __kmem_do_recursive_unmap(table, virt);

  kmem_invalidate_tlb_cache_entry(virt);

  return true;
}

// FIXME: free higher level pts as well
bool kmem_unmap_page(pml_entry_t* table, uintptr_t virt) {
  return kmem_unmap_page_ex(table, virt, 0);
}

bool kmem_unmap_range(pml_entry_t* table, uintptr_t virt, size_t page_count) {
  return kmem_unmap_range_ex(table, virt, page_count, NULL);
}

bool kmem_unmap_range_ex(pml_entry_t* table, uintptr_t virt, size_t page_count, uint32_t custom_flags)
{
  virt = ALIGN_DOWN(virt, SMALL_PAGE_SIZE);

  for (uintptr_t i = 0; i < page_count; i++) {

    if (!kmem_unmap_page_ex(table, virt, custom_flags))
      return false;

    virt += SMALL_PAGE_SIZE;
  }

  return true;
}

/*
 * NOTE: this function sets the flags for a page entry, not
 * any other entries like page directory entries or any of the such.
 *
 * NOTE: this also always sets the PRESENT bit to true
 */
void kmem_set_page_flags(pml_entry_t *page, uint32_t flags) 
{
  /* Make sure PAT is enabled if we want different caching options */
  if ((flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE || (flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH)
    flags |= KMEM_FLAG_SPEC;

  pml_entry_set_bit(page, PTE_PRESENT, true);
  pml_entry_set_bit(page, PTE_USER, ((flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL));
  pml_entry_set_bit(page, PTE_WRITABLE, ((flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE));
  pml_entry_set_bit(page, PTE_NO_CACHE, ((flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE));
  pml_entry_set_bit(page, PTE_WRITE_THROUGH, ((flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH));
  pml_entry_set_bit(page, PTE_PAT, ((flags & KMEM_FLAG_SPEC) == KMEM_FLAG_SPEC));
  pml_entry_set_bit(page, PTE_NX, ((flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE));
}

/*!
 * @brief Check if the specified process has access to an address
 *
 * when process is null, we simply use the kernel process
 */
ErrorOrPtr kmem_validate_ptr(struct proc* process, vaddr_t v_address, size_t size)
{
  vaddr_t v_offset;
  size_t p_page_count;
  pml_entry_t* root;
  pml_entry_t* page;

  root = (process ? (process->m_root_pd.m_root) : nullptr);

  p_page_count = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

  for (uint64_t i = 0; i < p_page_count; i++) {
    v_offset = v_address + (i * SMALL_PAGE_SIZE);

    if (v_offset >= HIGH_MAP_BASE)
      return Error();

    page = kmem_get_page(root, v_offset, NULL, NULL);

    /* Should be mapped AND should be mapped as user */
    if (!page || !pml_entry_is_bit_set(page, PTE_USER | PTE_PRESENT)) 
      return Error();

    /* TODO: check if the physical address is within a reserved range */
    /* TODO: check if the process owns the virtual range in its resource map */
    //p_addr = kmem_to_phys(root, v_offset);
  }

  return Success(0);
}

ErrorOrPtr kmem_ensure_mapped(pml_entry_t* table, vaddr_t base, size_t size)
{
  pml_entry_t* page;
  vaddr_t vaddr;
  size_t page_count;

  page_count = GET_PAGECOUNT(base, size);

  for (uint64_t i = 0; i < page_count; i++) {

    vaddr = base + (i * SMALL_PAGE_SIZE);

    page = kmem_get_page(table, vaddr, NULL, NULL);

    if (!page)
      return Error();
  }

  return Success(0);
}

/*
 * NOTE: About the 'current_driver' mechanism
 * 
 * We track allocations of drivers by keeping track of which driver we are currently executing, but there is nothing stopping the
 * drivers (yet) from using any kernel function that bypasses this mechanism, so mallicious (or simply malfunctioning) drivers can
 * cause us to lose memory at a big pase. We should (TODO) limit drivers to only use the functions that are deemed safe to use by drivers,
 * AKA we should blacklist any 'unsafe' functions, like __kmem_alloc_ex, which bypasses any resource tracking
 */

ErrorOrPtr __kmem_kernel_dealloc(uintptr_t virt_base, size_t size)
{
  return __kmem_dealloc(nullptr, nullptr, virt_base, size);
}

ErrorOrPtr __kmem_dealloc(pml_entry_t* map, kresource_bundle_t* resources, uintptr_t virt_base, size_t size) 
{
  /* NOTE: process is nullable, so it does not matter if we are not in a process at this point */
  return __kmem_dealloc_ex(map, resources, virt_base, size, false, false, false);
}

ErrorOrPtr __kmem_dealloc_unmap(pml_entry_t* map, kresource_bundle_t* resources, uintptr_t virt_base, size_t size) 
{
  return __kmem_dealloc_ex(map, resources, virt_base, size, true, false, false);
}

/*!
 * @brief Expanded deallocation function
 *
 */
ErrorOrPtr __kmem_dealloc_ex(pml_entry_t* map, kresource_bundle_t* resources, uintptr_t virt_base, size_t size, bool unmap, bool ignore_unused, bool defer_res_release) 
{
  const size_t pages_needed = GET_PAGECOUNT(virt_base, size);

  for (uintptr_t i = 0; i < pages_needed; i++) {
    // get the virtual address of the current page
    const vaddr_t vaddr = virt_base + (i * SMALL_PAGE_SIZE);
    // get the physical base of that page
    const paddr_t paddr = kmem_to_phys_aligned(map, vaddr);
    // get the index of that physical page
    const uintptr_t page_idx = kmem_get_page_idx(paddr);
    // check if this page is actually used
    const bool was_used = kmem_is_phys_page_used(page_idx);

    if (!was_used && !ignore_unused)
      return Warning();

    kmem_set_phys_page_free(page_idx);

    if (unmap && !kmem_unmap_page(map, vaddr))
      return Error();
  }

  /* Only release the resource if we dont want to defer that opperation */
  if (resources && !defer_res_release && IsError(resource_release(virt_base, size, GET_RESOURCE(resources, KRES_TYPE_MEM))))
    return Error();

  return Success(0);
}

/*!
 * @brief Allocate a physical address for the kernel
 *
 */
ErrorOrPtr __kmem_kernel_alloc(uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  return __kmem_alloc(nullptr, nullptr, addr, size, custom_flags, page_flags);
}

/*!
 * @brief Allocate a range of memory for the kernel
 *
 */
ErrorOrPtr __kmem_kernel_alloc_range (size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  return __kmem_alloc_range(nullptr, nullptr, KERNEL_MAP_BASE, size, custom_flags, page_flags);
}

ErrorOrPtr __kmem_dma_alloc(uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
  return __kmem_alloc_ex(nullptr, nullptr, addr, IO_MAP_BASE, size, custom_flags, page_flags);
}

ErrorOrPtr __kmem_dma_alloc_range(size_t size, uint32_t custom_flags, uint32_t page_flags)
{
  return __kmem_alloc_range(nullptr, nullptr, IO_MAP_BASE, size, custom_flags, page_flags);
}

ErrorOrPtr __kmem_alloc(pml_entry_t* map, kresource_bundle_t* resources, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  return __kmem_alloc_ex(map, resources, addr, KERNEL_MAP_BASE, size, custom_flags, page_flags);
}

ErrorOrPtr __kmem_alloc_ex(pml_entry_t* map, kresource_bundle_t* resources, paddr_t addr, vaddr_t vbase, size_t size, uint32_t custom_flags, uintptr_t page_flags) 
{
  const bool should_identity_map = (custom_flags & KMEM_CUSTOMFLAG_IDENTITY)  == KMEM_CUSTOMFLAG_IDENTITY;
  const bool should_remap = (custom_flags & KMEM_CUSTOMFLAG_NO_REMAP) != KMEM_CUSTOMFLAG_NO_REMAP;

  const paddr_t phys_base = ALIGN_DOWN(addr, SMALL_PAGE_SIZE);
  const size_t pages_needed = GET_PAGECOUNT(addr, size);

  const vaddr_t virt_base = should_identity_map ? phys_base : (should_remap ? kmem_from_phys(phys_base, vbase) : vbase);

  /* Compute return value since we align the parameter */
  const vaddr_t ret = should_identity_map ? addr : (should_remap ? kmem_from_phys(addr, vbase) : vbase);

  /*
   * Mark our pages as used BEFORE we map the range, since map_page
   * sometimes yoinks pages for itself 
   */
  kmem_set_phys_range_used(kmem_get_page_idx(phys_base), pages_needed);

  /* Don't mark, since we have already done that */
  if (!kmem_map_range(map, virt_base, phys_base, pages_needed, KMEM_CUSTOMFLAG_NO_MARK | KMEM_CUSTOMFLAG_GET_MAKE | custom_flags, page_flags))
    return Error();

  if (resources) {
    /*
     * NOTE: make sure to claim the range that we actually plan to use here, not what is actually 
     * allocated internally. This is because otherwise we won't be able to find this resource again if we 
     * try to release it
     */
    resource_claim_ex("kmem alloc", nullptr, ALIGN_DOWN(ret, SMALL_PAGE_SIZE), pages_needed * SMALL_PAGE_SIZE, KRES_TYPE_MEM, resources);
  }

  return Success(ret);
}

ErrorOrPtr __kmem_alloc_range(pml_entry_t* map, kresource_bundle_t* resources, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  ErrorOrPtr result;
  const size_t pages_needed = GET_PAGECOUNT(vbase, size);
  const bool should_identity_map = (custom_flags & KMEM_CUSTOMFLAG_IDENTITY)  == KMEM_CUSTOMFLAG_IDENTITY;
  const bool should_remap = (custom_flags & KMEM_CUSTOMFLAG_NO_REMAP) != KMEM_CUSTOMFLAG_NO_REMAP;

  result = bitmap_find_free_range(KMEM_DATA.m_phys_bitmap, pages_needed);

  if (IsError(result))
    return result;

  const uintptr_t phys_idx = Release(result);
  const paddr_t addr = kmem_get_page_addr(phys_idx);

  const paddr_t phys_base = addr;
  const vaddr_t virt_base = should_identity_map ? phys_base : (should_remap ? kmem_from_phys(phys_base, vbase) : vbase);

  /*
   * Mark our pages as used BEFORE we map the range, since map_page
   * sometimes yoinks pages for itself 
   */
  kmem_set_phys_range_used(phys_idx, pages_needed);

  if (!kmem_map_range(map, virt_base, phys_base, pages_needed, KMEM_CUSTOMFLAG_NO_MARK | KMEM_CUSTOMFLAG_GET_MAKE | custom_flags, page_flags))
    return Error();

  if (resources)
    resource_claim_ex("kmem alloc range", nullptr, ALIGN_DOWN(virt_base, SMALL_PAGE_SIZE), (pages_needed * SMALL_PAGE_SIZE), KRES_TYPE_MEM, resources);

  return Success(virt_base);
}

/*
 * This function will never remap or use identity mapping, so
 * KMEM_CUSTOMFLAG_NO_REMAP and KMEM_CUSTOMFLAG_IDENTITY are ignored here
 */
ErrorOrPtr __kmem_map_and_alloc_scattered(pml_entry_t* map, kresource_bundle_t* resources, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags) 
{
  ErrorOrPtr res;
  paddr_t p_addr;
  vaddr_t v_addr;

  const vaddr_t v_base = ALIGN_DOWN(vbase, SMALL_PAGE_SIZE);
  const size_t pages_needed = GET_PAGECOUNT(vbase, size);

  /*
   * 1) Loop for as many times as we need pages
   * 1.1)  - Find a physical page to map
   * 1.2)  - Map the physical page to its corresponding virtual address
   * 2) Claim the resource (virtual addressrange)
   */
  for (uint64_t i = 0; i < pages_needed; i++) {
  
    res = kmem_prepare_new_physical_page();

    if (IsError(res))
      return res;

    p_addr = Release(res);
    v_addr = v_base + (i * SMALL_PAGE_SIZE);

    /*
     * NOTE: don't mark, because otherwise the physical pages gets
     * marked free again after it is mapped
     */
    if (!kmem_map_page(
          map,
          v_addr,
          p_addr,
          KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_NO_MARK |
          KMEM_CUSTOMFLAG_NO_PHYS_REALIGN | custom_flags,
          page_flags)) {
      return Error();
    }
  }

  if (resources) {
    resource_claim_ex("kmem alloc scattered", nullptr, v_base, pages_needed * SMALL_PAGE_SIZE, KRES_TYPE_MEM, resources);
  }

  return Success(vbase);
}

/*!
 * @brief: Allocate a scattered bit of memory for a userprocess
 *
 */
ErrorOrPtr kmem_user_alloc_range(struct proc* p, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
  ErrorOrPtr result;
  uintptr_t first_usable_base;

  if (!p || !size)
    return Error();

  /* FIXME: this is not very safe, we need to randomize the start of process data probably lmaoo */
  result = resource_find_usable_range(p->m_resource_bundle, KRES_TYPE_MEM, size);

  if (IsError(result))
    return result;

  first_usable_base = Release(result);

  /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
  return __kmem_map_and_alloc_scattered(
        p->m_root_pd.m_root,
        p->m_resource_bundle,
        first_usable_base,
        size,
        custom_flags | KMEM_CUSTOMFLAG_CREATE_USER,
        page_flags 
        );
}

ErrorOrPtr kmem_user_dealloc(struct proc* p, vaddr_t vaddr, size_t size)
{
  kernel_panic("TODO: kmem_user_dealloc");
}

/*!
 * @brief: Allocate a block of memory into a userprocess
 * 
 */
ErrorOrPtr kmem_user_alloc(struct proc* p, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
  ErrorOrPtr result;
  uintptr_t first_usable_base;

  if (!p || !size)
    return Error();

  /* FIXME: this is not very safe, we need to randomize the start of process data probably lmaoo */
  result = resource_find_usable_range(p->m_resource_bundle, KRES_TYPE_MEM, size);

  if (IsError(result))
    return result;

  first_usable_base = Release(result);

  return __kmem_alloc_ex(
      p->m_root_pd.m_root,
      p->m_resource_bundle,
      addr,
      first_usable_base,
      size,
      custom_flags | KMEM_CUSTOMFLAG_CREATE_USER,
      page_flags);
}

/*!
 * @brief Fixup the mapping from boot.asm
 *
 * boot.asm maps 1 Gib high, starting from address NULL, so we'll do the same 
 * here to make sure the entire system stays happy after the pagemap switch
 */
static void __kmem_map_kernel_range_to_map(pml_entry_t* map) 
{
  const size_t max_end_idx = kmem_get_page_idx(2ULL * Gib);
  size_t mapping_end_idx = KMEM_DATA.m_phys_pages_count - 1;

  if (mapping_end_idx > max_end_idx)
    mapping_end_idx = max_end_idx;
  
  /* Map everything until what we've already mapped in boot.asm */
  ASSERT_MSG(
      kmem_map_range(
        map,
        HIGH_MAP_BASE,
        0,
        mapping_end_idx,
        KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_NO_MARK,
        KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL),
      "Failed to mmap pre-kernel memory"
      );
}

// TODO: make this more dynamic
static void _init_kmem_page_layout () 
{
  uintptr_t map;

  /* Grab the page for our pagemap root */
  map = Must(kmem_prepare_new_physical_page());

  /* Set the managers data */
  KMEM_DATA.m_kernel_base_pd = (pml_entry_t*)map;
  KMEM_DATA.m_high_page_base = HIGH_MAP_BASE;

  memset((void*)kmem_from_phys(map, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

  /* Assert that we got valid shit =) */
  ASSERT_MSG(kmem_get_krnl_dir(), "Tried to init kmem_page_layout without a present krnl dir");

  /* Mimic boot.asm */
  __kmem_map_kernel_range_to_map(nullptr);

  /* Load the new pagemap baby */
  kmem_load_page_dir(map, true);
}


/*!
 * @brief Create a new empty page directory that shares the kernel mappings
 *
 * Nothing to add here...
 */
page_dir_t kmem_create_page_dir(uint32_t custom_flags, size_t initial_mapping_size) {

  page_dir_t ret = { 0 };

  /* 
   * We can't just take a clean page here, since we need it mapped somewhere... 
   * For that we'll have to use the kernel allocation feature to instantly map
   * it somewhere for us in kernelspace =D
   */
  pml_entry_t* table_root = (pml_entry_t*)Must(__kmem_kernel_alloc_range(SMALL_PAGE_SIZE, 0, 0));

  /* Clear root, so we have no random mappings */
  memset(table_root, 0, SMALL_PAGE_SIZE);

  /* NOTE: this works, but I really don't want to have to do this =/ */
  Must(kmem_copy_kernel_mapping(table_root));

  const vaddr_t kernel_start = ALIGN_DOWN((uintptr_t)&_kernel_start, SMALL_PAGE_SIZE);
  const vaddr_t kernel_end = ALIGN_DOWN((uintptr_t)&_kernel_end, SMALL_PAGE_SIZE);

  ret.m_root = table_root;
  ret.m_kernel_low = kernel_start;
  ret.m_kernel_high = kernel_end;

  return ret;
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

  kernel_panic("TODO: implement kmem_to_current_pagemap");

  /* TODO: */
  return Error();
}

/*!
 * @brief Copy the high half of pml4 into the page directory root @new_table
 *
 * Nothing to add here...
 */
ErrorOrPtr kmem_copy_kernel_mapping(pml_entry_t* new_table) 
{
  bool is_present;
  pml_entry_t* kernel_root;
  pml_entry_t* kernel_lvl_3;

  const uintptr_t kernel_pml4_idx = 256;
  const uintptr_t end_idx = 511;

  kernel_root = (void*)kmem_from_phys((uintptr_t)kmem_get_krnl_dir(), KMEM_DATA.m_high_page_base);

  for (uintptr_t i = kernel_pml4_idx; i <= end_idx; i++) {

    /* Grab the pml entries */
    kernel_lvl_3 = &kernel_root[i];

    /* Check again (If this exists we'll be fine to copy everything) */
    is_present = pml_entry_is_bit_set(kernel_lvl_3, PDE_PRESENT);

    if (!is_present)
      continue;

    new_table[i].raw_bits = kernel_lvl_3->raw_bits;
  }

  return Success(0);
}

/*!
 * @brief Clear the shared kernel mappings from a page directory
 *
 * We share everything from pml4 index 256 and above with user processes.
 */
static ErrorOrPtr __clear_shared_kernel_mapping(pml_entry_t* dir)
{
  bool is_present;
  pml_entry_t* kernel_lvl_3;

  const uintptr_t kernel_pml4_idx = 256;
  const uintptr_t end_idx = 511;

  for (uintptr_t i = kernel_pml4_idx; i <= end_idx; i++) {
    /* Grab the pml entries */
    kernel_lvl_3 = &dir[i];

    /* Check again */
    is_present = pml_entry_is_bit_set(kernel_lvl_3, PDE_PRESENT);

    if (!is_present)
      continue;

    dir[i].raw_bits = NULL;
  }

  return Success(0);
}

/*
 * TODO: do we want to murder an entire addressspace, or do we just want to get rid 
 * of the mappings?
 * FIXME: with the current implementation we are definately bleeding memory, so another
 * TODO: plz fix the resource allocator asap so we can systematically find what mapping we can 
 *       murder and which we need to leave alive
 *
 * NOTE: Keep this function in mind, it might be really buggy, indicated by the comments above written
 * by me from 1 year ago or something lmao
 */
ErrorOrPtr kmem_destroy_page_dir(pml_entry_t* dir) 
{
  //if (__is_current_pagemap(dir))
  //  return Error();

  /*
   * We don't care about the result here
   * since it only fails if there is no kernel
   * entry, which is fine
   */
  __clear_shared_kernel_mapping(dir);

  /*
   * How do we want to clear our mappings?
   * 1) We recursively go through each level in the pagetable. 
   *    when we find an entry that is presens and its not at the bottom
   *    level of the pagetable, we go a level deeper and recurse
   * 2) At each mapping, we check if its shared of some sort (aka, we should not clear it)
   *    if it is not shared, we can clear the mapping
   * 3) When we have cleared an entire pde, pte, ect. we can free up that page. Since a pagetable is just a tree of
   *    pages that contain mappings, we should make sure we find all the pages that contain clearable mappings,
   *    so that we can return them to the physical allocator, for them to be reused. Otherwise we 
   *    keep these actually unused pages marked as 'used' by the allocator and we thus bleed memory
   *    which is not so nice =)
   */

  const uintptr_t dir_entry_limit = SMALL_PAGE_SIZE / sizeof(pml_entry_t);

  for (uintptr_t i = 0; i < dir_entry_limit; i++) {

    /* Check the root pagedir entry (pml4 entry) */
    if (!pml_entry_is_bit_set(&dir[i], PDE_PRESENT))
      continue;

    paddr_t pml4_entry_phys = kmem_get_page_base(dir[i].raw_bits);
    pml_entry_t* pml4_entry = (pml_entry_t*)kmem_ensure_high_mapping(pml4_entry_phys);

    for (uintptr_t j = 0; j < dir_entry_limit; j++) {

      /* Check the second layer pagedir entry (pml3 entry) */
      if (!pml_entry_is_bit_set(&pml4_entry[j], PDE_PRESENT))
        continue;

      paddr_t pdp_entry_phys = kmem_get_page_base(pml4_entry[j].raw_bits);
      pml_entry_t* pdp_entry = (pml_entry_t*)kmem_ensure_high_mapping(pdp_entry_phys);

      for (uintptr_t k = 0; k < dir_entry_limit; k++) {

        /* Check the third layer pagedir entry (pml2 entry) */
        if (!pml_entry_is_bit_set(&pdp_entry[k], PDE_PRESENT))
          continue;

        paddr_t pd_entry_phys = kmem_get_page_base(pdp_entry[k].raw_bits);
        pml_entry_t* pd_entry = (pml_entry_t*)kmem_ensure_high_mapping(pd_entry_phys);

        for (uintptr_t l = 0; l < dir_entry_limit; l++) {

          /* Check the fourth layer pagedir entry (pml1 entry) */
          if (!pml_entry_is_bit_set(&pd_entry[l], PTE_PRESENT))
            continue;

          /* Now we've reached the pagetable entry. If it's present we can mark it free */
          paddr_t p_pt_entry = kmem_get_page_base(pd_entry[l].raw_bits);
          uintptr_t idx = kmem_get_page_idx(p_pt_entry);
          kmem_set_phys_page_free(idx);
        } // pt loop

        /* Clear the pagetable */
        memset(pd_entry, 0, SMALL_PAGE_SIZE);

        uintptr_t idx = kmem_get_page_idx(pd_entry_phys);
        kmem_set_phys_page_free(idx);

      } // pd loop

      uintptr_t idx = kmem_get_page_idx(pdp_entry_phys);
      kmem_set_phys_page_free(idx);

    } // pdp loop

    uintptr_t idx = kmem_get_page_idx(pml4_entry_phys);
    kmem_set_phys_page_free(idx);

  } // pml4 loop

  paddr_t dir_phys = kmem_to_phys_aligned(nullptr, (uintptr_t)dir);
  uintptr_t idx = kmem_get_page_idx(dir_phys);

  kmem_set_phys_page_free(idx);

  return Success(0);
}

/*
 * NOTE: caller needs to ensure that they pass a physical address
 * as page map. CR3 only takes physical addresses
 */
void kmem_load_page_dir(paddr_t dir, bool __disable_interrupts) {
  if (__disable_interrupts) 
    disable_interrupts();

  pml_entry_t* page_map = (pml_entry_t*)dir;

  ASSERT(get_current_processor() != nullptr);
  get_current_processor()->m_page_dir = page_map;

  asm volatile("" : : : "memory");
  asm volatile("movq %0, %%cr3" ::"r"(dir));
  asm volatile("" : : : "memory");
}

ErrorOrPtr kmem_get_kernel_address(vaddr_t virtual_address, pml_entry_t* map) 
{
  return kmem_get_kernel_address_ex(virtual_address, KMEM_DATA.m_high_page_base, map);
}

ErrorOrPtr kmem_get_kernel_address_ex(vaddr_t virtual_address, vaddr_t map_base, pml_entry_t* map)
{
  pml_entry_t* page;
  paddr_t p_address;
  vaddr_t v_kernel_address;
  vaddr_t v_align_delta;

  /* Can't grab from NULL */
  if (!map) 
    return Error();

  /* Make sure we don't make a new page here */
  page = kmem_get_page(map, virtual_address, 0, 0);

  if (!page)
    return Error();

  p_address = kmem_get_page_base(page->raw_bits);

  /* Calculate the delta of the virtual address to its closest page base downwards */
  v_align_delta = virtual_address - ALIGN_DOWN(virtual_address, SMALL_PAGE_SIZE);

  /*
   * FIXME: 'high' mappings have a hard limit in them, so we will have to 
   * create some kind of dynamic mapping for certain types of memory. 
   * For example:
   *  - we map driver memory at a certain base for driver memory * the driver index
   *  - we map kernel resources at a certain base just for kernel resources
   *  - ect.
   */
  v_kernel_address = kmem_from_phys(p_address, map_base);

  /* Make sure the return the correctly aligned address */
  return Success(v_kernel_address + v_align_delta);
}

list_t const* kmem_get_phys_ranges_list() 
{
  return KMEM_DATA.m_phys_ranges;
}

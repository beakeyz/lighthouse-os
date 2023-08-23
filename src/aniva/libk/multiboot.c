#include "multiboot.h" 
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>

#define MB_MOD_PROBE_LOOPLIM 32
#define MB_TAG_PROBE_LOOPLIM 128

ErrorOrPtr init_multiboot(void* addr) {

  // first: find the memorymap 
  struct multiboot_tag_mmap* mb_memmap = get_mb2_tag(addr, 6);
  if (!mb_memmap) {
    println("No memorymap found! aborted");
    return Error();
  }

  /* NOTE: addr is virtual */
  g_system_info.total_multiboot_size = ALIGN_UP(get_total_mb2_size(addr) + 8, SMALL_PAGE_SIZE);

  return Success(0);
}

/*
 * Called after kmem_manager is set up, since we need to have access to the physical
 * allocator here. This function will preserve any essensial multiboot memory so that
 * it does not get overwritten by any allocations
 */
ErrorOrPtr finalize_multiboot(void* addr) {

  const size_t mb_pagecount = GET_PAGECOUNT(g_system_info.total_multiboot_size);
  const paddr_t aligned_mb_start = ALIGN_DOWN((uintptr_t)addr, SMALL_PAGE_SIZE);
  const uintptr_t mb_start_idx = kmem_get_page_idx(aligned_mb_start); 

  /* Reserve multiboot memory */
  kmem_set_phys_range_used(mb_start_idx, mb_pagecount);

  struct multiboot_tag_module* mod = g_system_info.ramdisk;

  /* Mark the ramdisk 'module' memory as used */
  if (mod && mod->mod_start && mod->mod_end && mod->mod_end > mod->mod_start) {
    const paddr_t module_start_idx = kmem_get_page_idx(ALIGN_DOWN(mod->mod_start, SMALL_PAGE_SIZE));
    const paddr_t module_end_idx = kmem_get_page_idx(ALIGN_UP(mod->mod_end, SMALL_PAGE_SIZE));

    const size_t module_size_pages = module_end_idx - module_start_idx;

    kmem_set_phys_range_used(module_start_idx, module_size_pages);
  }

  struct multiboot_tag_framebuffer* fbuffer = g_system_info.firmware_fb;

  /* Mark the framebuffer memory as used */
  if (fbuffer && fbuffer->common.framebuffer_addr) {

    println("Marking framebuffer as used!");
    uintptr_t phys_fb_start_idx = kmem_get_page_idx(ALIGN_DOWN(fbuffer->common.framebuffer_addr, SMALL_PAGE_SIZE));
    uintptr_t phys_fb_page_count = kmem_get_page_idx(ALIGN_UP(fbuffer->common.framebuffer_pitch * fbuffer->common.framebuffer_height, SMALL_PAGE_SIZE));

    kmem_set_phys_range_used(phys_fb_start_idx, phys_fb_page_count);
  }

  return Success(0);
}

size_t get_total_mb2_size(void* start_addr) {
  size_t ret = 0;
  void* idxPtr = ((void*)start_addr) + 8;
  struct multiboot_tag* tag = idxPtr;
  // loop through the tags to find the right type
  while (true) {
    //if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) break;
    if (tag->type == 0) break;

    ret += tag->size;

    idxPtr += tag->size;
    idxPtr = (void*)ALIGN_UP((uintptr_t)idxPtr, 8);
    tag = idxPtr;
  }

  return ret;
}

void* next_mb2_tag(void *cur, uint32_t type) {
  uintptr_t idxPtr = (uintptr_t)cur;
  struct multiboot_tag* tag = (void*)idxPtr;
  // loop through the tags to find the right type
  while (true) {
    if (tag->type == type) return tag;
    if (tag->type == 0) return nullptr;

    idxPtr += tag->size;

    idxPtr = ALIGN_UP(idxPtr, 8);
    tag = (void*)idxPtr;
  }
} 

void* get_mb2_tag(void *addr, uint32_t type) {
  if (!addr)
    return nullptr;

  char* header = ((void*) addr) + 8;
  return next_mb2_tag(header, type);
}

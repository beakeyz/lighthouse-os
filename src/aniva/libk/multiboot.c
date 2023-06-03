#include "multiboot.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>

ErrorOrPtr mb_initialize(void *addr) {

  // first: find the memorymap 
  struct multiboot_tag_mmap* mb_memmap = get_mb2_tag(addr, 6);
  if (!mb_memmap) {
    println("No memorymap found! aborted");
    return Error();
  }

  /* TODO: do we need to do anything else to init multiboot for the kernel? */

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
    while ((uintptr_t)idxPtr & 7) idxPtr++;
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
    while ((uintptr_t)idxPtr & 7) idxPtr++;
    tag = (void*)idxPtr;
  }
} 

void* get_mb2_tag(void *addr, uint32_t type) {
  if (!addr)
    return nullptr;

  char* header = ((void*) addr) + 8;
  return next_mb2_tag(header, type);
}

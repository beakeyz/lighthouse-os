#include "parser.h"
#include "dev/debug/serial.h"
#include "libk/linkedlist.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>
#include <libk/string.h>

void init_acpi_parser() {

}


void* find_rsdt () {

  const char* rsdt_sig = "RSD PTR ";
  // TODO: check other spots

  const uintptr_t bios_start_addr = 0xe0000;
  const size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  uintptr_t ptr = (uintptr_t)kmem_kernel_alloc(bios_start_addr, bios_mem_size, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr != NULL) {
    println(to_string(bios_mem_size));
    for (uintptr_t i = ptr; i < ptr + bios_mem_size; i+=16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }
  }

  const list_t* phys_ranges = kmem_get_phys_ranges_list();
  ITTERATE(phys_ranges);

    phys_mem_range_t* range = itterator->data;
    println(to_string(range->type));

    if (range->type != PMRT_ACPI_NVS || range->type != PMRT_ACPI_RECLAIM) {
      SKIP_ITTERATION();
    }
    println("passed!");

    uintptr_t start = range->start;
    uintptr_t length = range->length;
  
    for (uintptr_t i = start; i < length; i+= 16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }

  ENDITTERATE(phys_ranges);

  return nullptr;
}

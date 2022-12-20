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

  const list_t* phys_ranges = kmem_get_phys_ranges_list();
  ITTERATE(phys_ranges);

    phys_mem_range_t* range = itterator->data;
    println("loopin");
    println(to_string(range->type));

    if (range->type != PMRT_ACPI_NVS || range->type != PMRT_ACPI_RECLAIM) {
      SKIP_ITTERATION();
    }
    println("passed!");

    uintptr_t start = range->start;
    uintptr_t length = range->length;
  
    for (uintptr_t i = start; i < length; i+= 16) {
      void* potential = (void*)i;
      if (!memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }

  ENDITTERATE(phys_ranges);

  return nullptr;
}

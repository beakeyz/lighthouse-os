#include "parser.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include <kmain.h>

acpi_parser_t g_acpi_parser = { nullptr, false };

void init_acpi_parser() {

  RSDP_t* rsdp = find_rsdp();

  if (rsdp == nullptr) {
    // FIXME: we're fucked lol
    kernel_panic("no rsdt found =(");
  }

  g_acpi_parser.m_rsdp = rsdp;
  g_acpi_parser.m_is_xsdp = (rsdp->revision >= 2);

}


void* find_rsdp() {

  const char* rsdt_sig = "RSD PTR ";
  // TODO: check other spots

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = get_mb2_tag((void*)g_GlobalSystemInfo.m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);

  if (new_ptr && new_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)new_ptr->rsdp, sizeof(uintptr_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    print("Multiboot has rsdp: ");
    println(to_string((uintptr_t)ptr));
    return ptr;
  }

  struct multiboot_tag_old_acpi* old_ptr = get_mb2_tag((void*)g_GlobalSystemInfo.m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);

  if (old_ptr && old_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)old_ptr->rsdp, sizeof(uintptr_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    print("Multiboot has rsdp: ");
    println(to_string((uintptr_t)ptr));
    return ptr;
  }

  // check bios mem
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
  
    for (uintptr_t i = start; i < (start + length); i+= 16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }

  ENDITTERATE(phys_ranges);

  return nullptr;
}

// find a table by poking in memory. 
// TODO: find a way to cache the memory locations for all the tables
void* find_table(const char* sig) {

  void* rsdp_addr = g_acpi_parser.m_rsdp;

  if (rsdp_addr == nullptr) {
    return nullptr;
  }

  if (strlen(sig) != 4) {
    return nullptr;
  }

  XSDP_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(XSDP_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2 && ptr->xsdt_addr) {
    // xsdt
    kernel_panic("TODO: implement higher revisions >=(");
    return nullptr;
  }

  RSDT_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(RSDT_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(SDT_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    RSDT_t* cur = (RSDT_t*)(uintptr_t)rsdt->tables[i]; 
    if (memcmp(cur->base.signature, sig, 4)) {
      return (void*)(uintptr_t)rsdt->tables[i];
    }
  }
  
  return nullptr;
}

void print_tables(void* rsdp_addr) {
  XSDP_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(XSDP_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2 && ptr->xsdt_addr) {
    // xsdt
    kernel_panic("TODO: implement higher revisions >=(");
  }

  RSDT_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(RSDT_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(SDT_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    RSDT_t* cur = (RSDT_t*)(uintptr_t)rsdt->tables[i]; 
    for (int i = 0; i < 4; i++) {
      putch(cur->base.signature[i]);
    }
    print(" ");
  }
}

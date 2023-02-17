#include "parser.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include <kmain.h>
#include "acpi_obj.h"

acpi_parser_t *g_parser_ptr;

#define RSDP_SIGNATURE "RSD PTR "
#define FADT_SIGNATURE "FACP"
#define DSDT_SIGNATURE "DSDT"
#define MADT_SIGNATURE "APIC"

void init_acpi_parser(acpi_parser_t* parser) {

  println("Starting ACPI parser");

  acpi_rsdp_t* rsdp = find_rsdp();

  if (rsdp == nullptr) {
    // FIXME: we're fucked lol
    kernel_panic("no rsdt found =(");
  }

  parser->m_rsdp = rsdp;
  parser->m_is_xsdp = (rsdp->revision >= 2);

  parser->m_fadt = find_table(parser, "FACP");

  if (!parser->m_fadt) {
    kernel_panic("Unable to find ACPI table FADT!");
  }

  const uint32_t hw_reduced = ((parser->m_fadt->flags >> 20) & 1);

  parser->m_ns_root_node = acpi_create_root();

  void* dsdt_table = nullptr;

  dsdt_table = (void*)(uintptr_t)parser->m_fadt->dsdt_ptr;

  draw_char(0, 108, to_string(parser->m_rsdp->revision)[0]);
  draw_char(0, 116, to_string(parser->m_fadt->header.revision)[0]);

  if (!dsdt_table)
    kernel_panic("Unable to find ACPI table DSDT!");

  acpi_aml_seg_t *dsdt_aml_segment = acpi_load_segment(dsdt_table, 0);

  acpi_init_state(&parser->m_state);

  draw_char(16, 100, 'l');
  parser_prepare_acpi_state(&parser->m_state, dsdt_aml_segment, parser->m_ns_root_node);

  draw_char(24, 100, 'l');
  acpi_delete_state(&parser->m_state);
  // TODO:

  draw_char(100, 100, 'A');

  g_parser_ptr = parser;
}

void* find_rsdp() {
  const char* rsdt_sig = "RSD PTR ";
  // TODO: check other spots

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = get_mb2_tag((void*)g_GlobalSystemInfo.m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);

  if (new_ptr && new_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)new_ptr->rsdp, sizeof(uintptr_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    print("Multiboot has xsdp: ");
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
    for (uintptr_t i = ptr; i < ptr + bios_mem_size; i+=16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }
  }

  const list_t* phys_ranges = kmem_get_phys_ranges_list();
  FOREACH(i, phys_ranges) {

    phys_mem_range_t *range = i->data;

    if (range->type != PMRT_ACPI_NVS || range->type != PMRT_ACPI_RECLAIM) {
      continue;
    }

    uintptr_t start = range->start;
    uintptr_t length = range->length;

    for (uintptr_t i = start; i < (start + length); i += 16) {
      void *potential = (void *) i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }
  }

  return nullptr;
}

// find a table by poking in memory. 
// TODO: find a way to cache the memory locations for all the tables
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index) {

  void* rsdp_addr = parser->m_rsdp;

  if (rsdp_addr == nullptr) {
    return nullptr;
  }

  if (strlen(sig) != 4) {
    return nullptr;
  }

  acpi_xsdp_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(acpi_xsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2) {
    // xsdt
    if (ptr->xsdt_addr) {
      acpi_xsdt_t* xsdt = kmem_kernel_alloc((uintptr_t)ptr->xsdt_addr, sizeof(acpi_xsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

      if (xsdt != nullptr) {
        const size_t end_index = ((xsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint64_t);

        for (uint64_t i = 0; i < end_index; i++) {
          acpi_rsdt_t* cur = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)xsdt->tables[i], sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
          if (memcmp(cur->base.signature, sig, 4) != 0) {
            if (index == 0) {
              return (void*)(uintptr_t)xsdt->tables[i];
            }
            index--;
          }
        }
      }
    } 
  }

  acpi_rsdt_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    acpi_rsdt_t* cur = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)rsdt->tables[i], sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    if (memcmp(cur->base.signature, sig, 4) != 0) {
      if (index == 0) {
        return (void*)(uintptr_t)rsdt->tables[i];
      }
      index--;
    }
  }
  
  return nullptr;
}

void* find_table(acpi_parser_t *parser, const char* sig) {
  return (void*)find_table_idx(parser, sig, 0);
}

void print_tables(void* rsdp_addr) {
  acpi_xsdp_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(acpi_xsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2 && ptr->xsdt_addr) {
    // xsdt
    kernel_panic("TODO: implement higher revisions >=(");
  }

  acpi_rsdt_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    acpi_rsdt_t* cur = (acpi_rsdt_t*)(uintptr_t)rsdt->tables[i];
    for (int i = 0; i < 4; i++) {
      putch(cur->base.signature[i]);
    }
    print(" ");
  }
}

/*
 * AML stuff
 */

int parser_prepare_acpi_state(acpi_state_t* state, acpi_aml_seg_t* segment, acpi_ns_node_t* parent_node) {

  // TODO: make sure our state-stacks have enough memory

  const size_t data_size = segment->aml_table->header.length - sizeof(acpi_sdt_header_t);

  // push a context
  acpi_context_entry_t* ctx = acpi_state_push_context_entry(state);
  ctx->segm = segment;
  ctx->code = segment->aml_table->data;
  ctx->ctx_handle = parent_node;

  // push a block
  acpi_block_entry_t* block = acpi_state_push_block_entry(state);
  block->program_counter = 0;
  block->program_counter_limit = data_size;

  // push a stackitem
  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_POPULATE_STACKITEM;
  
  // TODO: evaluate/exectute aml

  return 0;
}

int parser_execute_acpi_state(acpi_state_t* state) {

  return 0;
}

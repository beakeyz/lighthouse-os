#include "parser.h"
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/flow/reference.h"
#include "mem/pg.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "system/acpi/opcodes.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>

#define RSDP_SIGNATURE "RSD PTR "
#define FADT_SIGNATURE "FACP"
#define DSDT_SIGNATURE "DSDT"
#define MADT_SIGNATURE "APIC"
#define SSDT_SIGNATURE "SSDT"
#define PSDT_SIGNATURE "PSDT"

static acpi_parser_rsdp_discovery_method_t create_rsdp_method_state(enum acpi_rsdp_method method) {
  const char* name;
  switch (method) {
    case MULTIBOOT_NEW:
      name = "Multiboot XSDP header";
      break;
    case MULTIBOOT_OLD:
      name = "Multiboot RSDP header";
      break;
    case BIOS_POKE:
      name = "BIOS memory poking";
      break;
    case RECLAIM_POKE:
      name = "Reclaimable memory poking";
      break;
    case NONE:
      name = "None";
      break;
  }

  return (acpi_parser_rsdp_discovery_method_t) {
    .m_method = method,
    .m_name = name
  };
}

ErrorOrPtr create_acpi_parser(acpi_parser_t* parser) {
  println("Starting ACPI parser");

  parser->m_tables = init_list();

  ASSERT(find_rsdp(parser));

  if (parser->m_rsdp_discovery_method.m_method == NONE) {
    // FIXME: we're fucked lol
    // kernel_panic("no rsdt found =(");
    //return Error();
    return Error();
  }

  /*
   * Use the r/xsdp in order to find all the tables, 
   * cache their addresses and premap their headers
   */
  parser_init_tables(parser);

  parser->m_fadt = acpi_parser_find_table(parser, FADT_SIGNATURE, sizeof(acpi_fadt_t));

  if (!parser->m_fadt) {
    return Error();
  }

  //hw_reduced = ((parser->m_fadt->flags >> 20) & 1);

  println("Started");
  return Success(0);
}

/*
 * NOTE: We don't immediately map the entire table length once we find it. That is only done 
 * once we try to find a table using find_table_idx (which does not find a table index, but 
 * rather a table WITH an index so TODO: fix this naming inconsistency)
 */
void parser_init_tables(acpi_parser_t* parser) {
  acpi_xsdt_t* xsdt = nullptr;
  acpi_rsdt_t* rsdt = nullptr;
  acpi_sdt_header_t* header;
  size_t tables;
  uintptr_t table_entry;
  paddr_t phys;

  ASSERT_MSG(parser->m_rsdp || parser->m_xsdp, "No ACPI pointer found!");

  println("Lol");
  if (parser->m_xsdp) {
    xsdt = (acpi_xsdt_t*)Must(__kmem_kernel_alloc(parser->m_xsdp->xsdt_addr, sizeof(acpi_xsdt_t), NULL, 0));
    println("A");
    println(to_string((uintptr_t)xsdt));
    println(to_string((uintptr_t)parser->m_xsdp->xsdt_addr));
    println(to_string(parser->m_xsdp->length));
    println(to_string(xsdt->base.length));
    xsdt = (acpi_xsdt_t*)Must(__kmem_kernel_alloc(parser->m_xsdp->xsdt_addr, xsdt->base.length, NULL, 0));
    println("B");
    tables = (xsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uintptr_t);

    println(to_string(tables));
    for (uintptr_t i = 0; i < tables; i++) {

      if (!xsdt->tables[i])
        break;

      table_entry = xsdt->tables[i];
      phys = kmem_to_phys(nullptr, table_entry);

      if (phys && table_entry >= HIGH_MAP_BASE)
        table_entry = phys;

      header = (acpi_sdt_header_t*)Must(__kmem_kernel_alloc(table_entry, sizeof(acpi_sdt_header_t), NULL, 0));

      list_append(parser->m_tables, header);
    } 

    return;
  }

  rsdt = (acpi_rsdt_t*)Must(__kmem_kernel_alloc(parser->m_rsdp->rsdt_addr, sizeof(acpi_rsdt_t), NULL, 0));
  rsdt = (acpi_rsdt_t*)Must(__kmem_kernel_alloc(parser->m_rsdp->rsdt_addr, rsdt->base.length, NULL, 0));
  tables = (rsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);

  for (uintptr_t i = 0; i < tables; i++) {

      uint32_t table_entry = rsdt->tables[i];
      paddr_t phys = kmem_to_phys(nullptr, table_entry);

      if (phys)
        table_entry = phys;

      header = (acpi_sdt_header_t*)Must(__kmem_kernel_alloc(table_entry, sizeof(acpi_sdt_header_t), NULL, 0));

      list_append(parser->m_tables, header);
  } 
}

void* find_rsdp(acpi_parser_t* parser) {
  // TODO: check other spots
  uintptr_t pointer;

  parser->m_is_xsdp = false;
  parser->m_xsdp = nullptr;
  parser->m_rsdp = nullptr;
  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = g_system_info.xsdp;

  if (new_ptr) {
    /* check if ->rsdp is a virtual or physical address */
    pointer = (uintptr_t)new_ptr->rsdp;

    print("Multiboot has xsdp: ");
    println(to_string(pointer));
    parser->m_is_xsdp = true;
    parser->m_xsdp = (acpi_xsdp_t*)pointer;

    println(parser->m_xsdp->base.OEMID);
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_NEW);
    return parser->m_xsdp;
  }

  struct multiboot_tag_old_acpi* old_ptr = g_system_info.rsdp;

  if (old_ptr) {

    pointer = (uintptr_t)old_ptr->rsdp;

    print("Multiboot has rsdp: ");
    println(to_string(pointer));
    parser->m_rsdp = (acpi_rsdp_t*)pointer;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_OLD);
    return parser->m_rsdp;
  }

  // check bios mem
  uintptr_t bios_start_addr = 0xe0000;
  size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  if (IsError(kmem_ensure_mapped(nullptr, bios_start_addr, bios_mem_size)))
    bios_start_addr = Must(__kmem_kernel_alloc(bios_start_addr, bios_mem_size, NULL, NULL));

  for (uintptr_t i = bios_start_addr; i < bios_start_addr + bios_mem_size; i+=16) {
    acpi_xsdp_t* potential = (void *)i;
    if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
      parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);

      if (potential->base.revision >= 2 && potential->xsdt_addr) {
        parser->m_xsdp = potential;
        parser->m_is_xsdp = true;
      } else 
        parser->m_rsdp = (acpi_rsdp_t*)potential;

      return potential;
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
      acpi_xsdp_t* potential = (void *)i;
      if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
        parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);

        if (potential->base.revision >= 2 && potential->xsdt_addr) {
          parser->m_xsdp = potential;
          parser->m_is_xsdp = true;
        } else 
          parser->m_rsdp = (acpi_rsdp_t*)potential;

        return potential;
      }
    }
  }

  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);
  return nullptr;
}

void* acpi_parser_find_table_idx(acpi_parser_t *parser, const char* sig, size_t index, size_t table_size) {
  FOREACH(i, parser->m_tables) {
    acpi_sdt_header_t* header = i->data;
    if (memcmp(&header->signature, sig, 4)) {
      if (index == 0) {

        if (table_size)
          header = (acpi_sdt_header_t*)Must(__kmem_kernel_alloc(kmem_to_phys(nullptr, (uint64_t)header), table_size, NULL, NULL));

        return (void*)header;
      }
      index--;
    }
  }
  return nullptr;
}

void* acpi_parser_find_table(acpi_parser_t *parser, const char* sig, size_t table_size) {
  return (void*)acpi_parser_find_table_idx(parser, sig, 0, table_size);
}

const int parser_get_acpi_tables(acpi_parser_t* parser, char* names) {

  FOREACH(i, parser->m_tables) {
    acpi_sdt_header_t* header = i->data;

    char sig[5];
    memcpy(&sig, header->signature, 4);
    sig[4] = '\0';

    concat(names, sig, names);

    if (i->next)
      concat(names, ", ", names);
  }

  return 0;
}

void print_tables(acpi_parser_t* parser) {

  const char tables[parser->m_tables->m_length * 6];

  parser_get_acpi_tables(parser, (char*)tables);
  println("Tables");
  println(tables);
}

/*
 * AML stuff
 * TODO: redo
 */

void init_acpi_parser_aml(acpi_parser_t* parser) {

  // Initialize an empty namespace

  // Find all the controlblocks that contain AML

  // Go through every controlblock and parse the AML 
  // so to fill the namespace with a coherent structure 
  // of nodes that can be interacted with by other software
  // (like querying for certain data about pci addresses, for example)

  kernel_panic("UNIMPLEMENTED: redo init_acpi_parser_aml");
}

#include "parser.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
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

acpi_parser_t *g_parser_ptr;

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

ErrorOrPtr create_acpi_parser(acpi_parser_t* parser, uintptr_t multiboot_addr) {
  g_parser_ptr = parser;

  parser->m_multiboot_addr = multiboot_addr;
  parser->m_tables = init_list();

  println("Starting ACPI parser");

  find_rsdp(parser);
  println("Found rsdp");

  if (parser->m_rsdp_discovery_method.m_method == NONE) {
    // FIXME: we're fucked lol
    // kernel_panic("no rsdt found =(");
    return Error();
  }

  /*
   * Use the r/xsdp in order to find all the tables, 
   * cache their addresses and premap their headers
   */
  parser_init_tables(parser);

  parser->m_fadt = find_table(parser, FADT_SIGNATURE);

  if (!parser->m_fadt) {
    return Error();
  }

  //hw_reduced = ((parser->m_fadt->flags >> 20) & 1);

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
  size_t tables;

  if (parser->m_is_xsdp) {
    xsdt = (acpi_xsdt_t*)Must(__kmem_kernel_alloc(parser->m_xsdp->xsdt_addr, sizeof(acpi_xsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));
    xsdt = (acpi_xsdt_t*)Must(__kmem_kernel_alloc(kmem_to_phys(nullptr, (uintptr_t)xsdt), xsdt->base.length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));
    tables = (xsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uintptr_t);

    for (uintptr_t i = 0; i < tables; i++) {
      /* The table addresses should just be physical */
      acpi_sdt_header_t* table = (acpi_sdt_header_t*)Must(__kmem_kernel_alloc(xsdt->tables[i], sizeof(acpi_sdt_header_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));

      list_append(parser->m_tables, table);
    } 

    return;
  }

  rsdt = (acpi_rsdt_t*)Must(__kmem_kernel_alloc((uintptr_t)parser->m_rsdp->rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));
  rsdt = (acpi_rsdt_t*)Must(__kmem_kernel_alloc(kmem_to_phys(nullptr, (uintptr_t)rsdt), rsdt->base.length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));
  tables = (rsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);

  for (uintptr_t i = 0; i < tables; i++) {
    acpi_sdt_header_t* table = (acpi_sdt_header_t*)Must(__kmem_kernel_alloc(rsdt->tables[i], sizeof(acpi_sdt_header_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));

    list_append(parser->m_tables, table);
  } 
}

void* find_rsdp(acpi_parser_t* parser) {
  // TODO: check other spots
  parser->m_is_xsdp = false;
  parser->m_xsdp = nullptr;
  parser->m_rsdp = nullptr;
  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);

  if (new_ptr && new_ptr->rsdp[0]) {
    /* TODO: check if ->rsdp is a virtual or physical address */
    parser->m_is_xsdp = true;
    parser->m_xsdp = (void*)new_ptr->rsdp;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_NEW);
    return parser->m_xsdp;
  }

  struct multiboot_tag_old_acpi* old_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);

  if (old_ptr && old_ptr->rsdp[0]) {
    //print("Multiboot has rsdp: ");
    //println(to_string((uintptr_t)ptr));
    parser->m_rsdp = (void*)old_ptr->rsdp;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_OLD);
    return parser->m_rsdp;
  }

  // check bios mem
  const uintptr_t bios_start_addr = 0xe0000;
  const size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  uintptr_t ptr = (uintptr_t)Must(__kmem_kernel_alloc(bios_start_addr, bios_mem_size, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));

  if (ptr != NULL) {
    for (uintptr_t i = ptr; i < ptr + bios_mem_size; i+=16) {
      void* potential = (void*)i;
      if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
        parser->m_rsdp_discovery_method = create_rsdp_method_state(BIOS_POKE);
        parser->m_rsdp = potential;
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
      if (memcmp(RSDP_SIGNATURE, potential, strlen(RSDP_SIGNATURE))) {
        parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);
        parser->m_rsdp = potential;
        return potential;
      }
    }
  }

  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);
  return nullptr;
}

void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index) {
  FOREACH(i, parser->m_tables) {
    acpi_sdt_header_t* header = i->data;
    if (memcmp(&header->signature, sig, 4)) {
      if (index == 0) {
        header = (void*)Must(__kmem_kernel_alloc(kmem_to_phys(nullptr,(uintptr_t)header), header->length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0));
        return (void*)header;
      }
      index--;
    }
  }
  return nullptr;
}

void* find_table(acpi_parser_t *parser, const char* sig) {
  return (void*)find_table_idx(parser, sig, 0);
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

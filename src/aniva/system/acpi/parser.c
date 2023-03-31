#include "parser.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/reference.h"
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

static size_t parse_acpi_var_int(size_t* out, uint8_t *code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u8(uint8_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u16(uint16_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u32(uint32_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u64(uint64_t* out, uint8_t* code_ptr, int* pc, int limit);

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

  acpi_rsdp_t* rsdp;
  uint32_t hw_reduced;

  println("Starting ACPI parser");

  parser->m_multiboot_addr = multiboot_addr;
  parser->m_tables = init_list();
  // parser->m_namespace_nodes = create_hive("acpi");

  find_rsdp(parser);

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

  hw_reduced = ((parser->m_fadt->flags >> 20) & 1);
  return Success(0);
}

void parser_init_tables(acpi_parser_t* parser) {
  acpi_xsdt_t* xsdt = nullptr;
  acpi_rsdt_t* rsdt = nullptr;
  size_t tables;

  if (parser->m_is_xsdp) {
    xsdt = (acpi_xsdt_t*)kmem_kernel_alloc(parser->m_xsdp->xsdt_addr, sizeof(acpi_xsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    xsdt = (acpi_xsdt_t*)kmem_kernel_alloc((uintptr_t)xsdt, xsdt->base.length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    tables = (xsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uintptr_t);
  } else {
    rsdt = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)parser->m_rsdp->rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    rsdt = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)rsdt, rsdt->base.length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    tables = (rsdt->base.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
  }

  if (xsdt != nullptr) {
    // parse xsdt
    for (uintptr_t i = 0; i < tables; i++) {
      acpi_sdt_header_t* table = (acpi_sdt_header_t*)kmem_kernel_alloc(xsdt->tables[i], sizeof(acpi_sdt_header_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);

      list_append(parser->m_tables, table);
    } 
  } else {
    for (uintptr_t i = 0; i < tables; i++) {
      acpi_sdt_header_t* table = (acpi_sdt_header_t*)kmem_kernel_alloc(rsdt->tables[i], sizeof(acpi_sdt_header_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);

      list_append(parser->m_tables, table);
    } 
  }
}

void* find_rsdp(acpi_parser_t* parser) {
  const char* rsdt_sig = "RSD PTR ";
  // TODO: check other spots
  parser->m_is_xsdp = false;

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);

  if (new_ptr && new_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)new_ptr->rsdp, sizeof(acpi_xsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    //print("Multiboot has xsdp: ");
    //println(to_string((uintptr_t)ptr));
    parser->m_is_xsdp = true;
    parser->m_xsdp = ptr;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_NEW);
    return ptr;
  }

  struct multiboot_tag_old_acpi* old_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);

  if (old_ptr && old_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)old_ptr->rsdp, sizeof(acpi_rsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
    //print("Multiboot has rsdp: ");
    //println(to_string((uintptr_t)ptr));
    parser->m_rsdp = ptr;
    parser->m_rsdp_discovery_method = create_rsdp_method_state(MULTIBOOT_OLD);
    return ptr;
  }

  // check bios mem
  const uintptr_t bios_start_addr = 0xe0000;
  const size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  uintptr_t ptr = (uintptr_t)kmem_kernel_alloc(bios_start_addr, bios_mem_size, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);

  if (ptr != NULL) {
    for (uintptr_t i = ptr; i < ptr + bios_mem_size; i+=16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
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
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        parser->m_rsdp_discovery_method = create_rsdp_method_state(RECLAIM_POKE);
        parser->m_rsdp = potential;
        return potential;
      }
    }
  }

  parser->m_rsdp_discovery_method = create_rsdp_method_state(NONE);
  return nullptr;
}

// find a table by poking in memory. 
// TODO: find a way to cache the memory locations for all the tables
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index) {
  FOREACH(i, parser->m_tables) {
    acpi_sdt_header_t* header = i->data;
    if (memcmp(&header->signature, sig, 4)) {
      if (index == 0) {
        header = (void*)kmem_kernel_alloc((uintptr_t)header, header->length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY);
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


/*
 * ACPI opcode parsing functions
 */

static size_t parse_acpi_var_int(size_t* out, uint8_t *code_ptr, int* pc, int limit) {
  if (*pc + 1 > limit)
      return -1;
  uint8_t sz = (code_ptr[*pc] >> 6) & 3;
  if (!sz) {
    *out = (size_t)(code_ptr[*pc] & 0x3F);
    (*pc)++;
    return 0;
  } else if (sz == 1) {
    if (*pc + 2 > limit)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4);
    *pc += 2;
    return 0;
  } else if (sz == 2) {
    if (*pc + 3 > limit)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4)
           | (size_t)(code_ptr[*pc + 2] << 12);
    *pc += 3;
    return 0;
  } else {
    if (*pc + 4 > limit || sz != 3)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4)
           | (size_t)(code_ptr[*pc + 2] << 12) | (size_t)(code_ptr[*pc + 3] << 20);
    *pc += 4;
    return 0;
  }
}

static ALWAYS_INLINE int acpi_parse_u8(uint8_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 1 > limit) {
    return -1;
  }

  *out = code_ptr[*pc];
  (*pc)++;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u16(uint16_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 2 > limit) {
    return -1;
  }

  *out = ((uint16_t)code_ptr[*pc]) | (((uint16_t)code_ptr[*pc + 1]) << 8);
  *pc += 2;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u32(uint32_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 4 > limit) {
    return -1;
  }

  *out = ((uint32_t)code_ptr[*pc])              | (((uint32_t)code_ptr[*pc + 1]) << 8)
      |  (((uint32_t) code_ptr[*pc + 2]) << 16) | (((uint32_t) code_ptr[*pc + 3]) << 24);
  *pc += 4;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u64(uint64_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 8 > limit) {
    return -1;
  }

  *out = ((uint64_t)code_ptr[*pc])              | (((uint64_t)code_ptr[*pc + 1]) << 8)
      |  (((uint64_t) code_ptr[*pc + 2]) << 16) | (((uint64_t) code_ptr[*pc + 3]) << 24)
      |  (((uint64_t) code_ptr[*pc + 4]) << 32) | (((uint64_t) code_ptr[*pc + 5]) << 40)
      |  (((uint64_t) code_ptr[*pc + 6]) << 48) | (((uint64_t) code_ptr[*pc + 7]) << 56);
  *pc += 8;
  return 0;
}

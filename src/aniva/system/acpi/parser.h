#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "sync/spinlock.h"
#include "system/acpi/structures.h"

#define RSDP_SIGNATURE "RSD PTR "
#define FADT_SIGNATURE "FACP"
#define DSDT_SIGNATURE "DSDT"
#define MADT_SIGNATURE "APIC"
#define SSDT_SIGNATURE "SSDT"
#define PSDT_SIGNATURE "PSDT"

typedef struct {
  enum acpi_rsdp_method {
    MULTIBOOT_OLD,
    MULTIBOOT_NEW,
    BIOS_POKE,
    RECLAIM_POKE,
    NONE,
  } m_method;

  const char* m_name;
} acpi_parser_rsdp_discovery_method_t;

// TODO: fill structure
// TODO: should the acpi parser have its own heap?
typedef struct acpi_parser {
  acpi_rsdp_t* m_rsdp;
  acpi_xsdp_t* m_xsdp;
  paddr_t m_rsdp_phys;
  paddr_t m_xsdp_phys;
  
  bool m_is_xsdp;

  list_t* m_tables;

  acpi_fadt_t *m_fadt;
  //uintptr_t m_multiboot_addr;

  acpi_parser_rsdp_discovery_method_t m_rsdp_discovery_method;

  int m_acpi_rev;

} acpi_parser_t;

ErrorOrPtr create_acpi_parser(acpi_parser_t* parser);

void init_acpi_parser_aml(acpi_parser_t* parser);

void parser_init_tables(acpi_parser_t* parser);

// find rsdt and (if available) xsdt
void* find_rsdp(acpi_parser_t* parser);

// me want cool table
void* acpi_parser_find_table_idx(acpi_parser_t *parser, const char* sig, size_t index, size_t table_size);
void* acpi_parser_find_table(acpi_parser_t *parser, const char* sig, size_t table_size);

// just for funzies
void print_tables(acpi_parser_t* parser);

const int parser_get_acpi_tables(acpi_parser_t* parser, char* out);

/*
 * TODO: redo aml
 */
#endif // !

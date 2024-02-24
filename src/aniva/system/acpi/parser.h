#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "libk/data/linkedlist.h"
#include "system/acpi/acpica/actypes.h"
#include "tables.h"

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

/*
 * Our own local ACPI parser
 *
 * This is a little layer between aniva and ACPICA. When anything in the system wants to know
 * about acpi tables, evaluate AML, do anything ACPI, it goes here and then the parser queries
 * ACPICA
 */
typedef struct acpi_parser {
  acpi_tbl_rsdp_t* m_rsdp_table;
  acpi_tbl_rsdt_t* m_rsdt;
  paddr_t m_rsdp_phys;
  paddr_t m_xsdp_phys;
  paddr_t m_rsdt_phys;
  
  bool m_is_xsdp:1;
  bool m_is_acpi_enabled:1;
  int m_acpi_rev;

  /* Local cache for the static ACPI tables */
  list_t* m_tables;

  /* How did we find the R/Xsdp */
  acpi_parser_rsdp_discovery_method_t m_rsdp_discovery_method;
} acpi_parser_t;

int init_acpi_parser_early(acpi_parser_t* parser);
ErrorOrPtr init_acpi_parser(acpi_parser_t* parser);

static inline enum acpi_rsdp_method acpi_parser_get_rsdp_method(acpi_parser_t* p)
{
  return p->m_rsdp_discovery_method.m_method;
}

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

ACPI_STATUS acpi_eval_int(acpi_handle_t handle, ACPI_STRING string, ACPI_OBJECT_LIST* args, size_t* ret);

/*
 * TODO: redo aml
 */
#endif // !

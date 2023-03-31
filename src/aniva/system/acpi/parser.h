#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "dev/debug/serial.h"
#include "libk/hive.h"
#include "libk/linkedlist.h"
#include "structures.h"
#include "sync/spinlock.h"

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
  bool m_is_xsdp;

  list_t* m_tables;

  acpi_fadt_t *m_fadt;
  uintptr_t m_multiboot_addr;

  acpi_parser_rsdp_discovery_method_t m_rsdp_discovery_method;

  int m_acpi_rev;

} acpi_parser_t;

ErrorOrPtr create_acpi_parser(acpi_parser_t* parser, uintptr_t multiboot_addr);

void init_acpi_parser_aml(acpi_parser_t* parser);

void parser_init_tables(acpi_parser_t* parser);

// find rsdt and (if available) xsdt
void* find_rsdp(acpi_parser_t* parser);

// me want cool table
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index);
void* find_table(acpi_parser_t *parser, const char* sig);

// just for funzies
void print_tables(acpi_parser_t* parser);

const int parser_get_acpi_tables(acpi_parser_t* parser, char* out);

/*
 * TODO: redo aml
 */

extern acpi_parser_t *g_parser_ptr;
#endif // !

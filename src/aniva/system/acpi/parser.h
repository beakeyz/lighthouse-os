#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "structures.h"
#include "acpi_obj.h"
#include "namespace.h"

// TODO: fill structure
typedef struct {
  acpi_rsdp_t* m_rsdp;
  acpi_fadt_t *m_fadt;
  bool m_is_xsdp;

  int m_acpi_rev;

  acpi_state_t m_state;

  // acpi aml namespace nodes
  acpi_ns_node_t* m_ns_root_node;
  acpi_ns_node_t** m_ns_nodes;

  size_t m_ns_size;
  size_t m_ns_max_size;

} acpi_parser_t;

void init_acpi_parser(acpi_parser_t* parser);

int acpi_init_state(acpi_state_t* state);
int acpi_delete_state(acpi_state_t* state);

// find rsdt and (if available) xsdt
void* find_rsdp();

// me want cool table
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index);
void* find_table(acpi_parser_t *parser, const char* sig);

// just for funzies
void print_tables(void* rsdp_addr);

// TODO: find needed params
void start_aml_parsing();

extern acpi_parser_t *g_parser_ptr;
#endif // !

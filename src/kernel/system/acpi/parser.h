#ifndef __LIGHT_ACPI_PARSER__ 
#define __LIGHT_ACPI_PARSER__
#include <libk/stddef.h>

// TODO: fill structure
typedef struct {
  void* m_rsdp;
  bool m_is_xsdp;
} acpi_parser_t;

void init_acpi_parser();

// find rsdt and (if available) xsdt
void* find_rsdp();

// me want cool table
void* find_table(void* rsdp_addr, const char* sig);

// just for funzies
void print_tables(void* rsdp_addr);

// TODO: find needed params
void start_aml_parsing();


#endif // !

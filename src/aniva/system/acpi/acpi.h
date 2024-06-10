#ifndef __ANIVA_ACPI__
#define __ANIVA_ACPI__

#include "system/acpi/acpica/actypes.h"
#include <libk/stddef.h>

struct acpi_parser;

void init_acpi();
void init_acpi_core();
bool is_acpi_init();

void get_root_acpi_parser(struct acpi_parser** out);
void* find_acpi_table(char* signature, size_t table_size);
paddr_t find_acpi_root_ptr();

extern ACPI_STATUS init_acpi_early();
extern void init_acpi_devices();

#endif // !

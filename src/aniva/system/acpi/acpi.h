#ifndef __ANIVA_ACPI__ 
#define __ANIVA_ACPI__

#include <libk/stddef.h>

struct acpi_parser;

void init_acpi();
bool is_acpi_init();

void get_root_acpi_parser(struct acpi_parser* out);

#endif // !

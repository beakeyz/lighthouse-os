#ifndef __ANIVA_ACPI__ 
#define __ANIVA_ACPI__

#include <libk/stddef.h>

void init_acpi(uintptr_t multiboot_addr);
bool is_acpi_init();

#endif // !

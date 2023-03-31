#include "acpi.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/pci/pci.h"
#include "libk/error.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>

acpi_parser_t parser;

void init_acpi(uintptr_t multiboot_addr) {

  Must(create_acpi_parser(&parser, multiboot_addr));

  // TODO: check FADT table for irq shit and verify that the parser booted up nicely
  // TODO: the purpose of this stub is to verify what the parser did while booting 
  // and making sure the rest of the boot happens correctly, based on what the parser
  // is able to find and use. For example, if it does not find an MADT, we should inform
  // the kernel and boot with legacy PIC
}

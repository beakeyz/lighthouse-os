#include "acpi.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/pci/pci.h"
#include "libk/error.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>

void init_acpi() {

  init_acpi_parser();

  FADT_t* t = find_table("FACP");

  if (t != nullptr) {
    return;
  }

  // TODO: check FADT table for irq shit and verify that the parser booted up nicely
}

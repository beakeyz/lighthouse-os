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
  draw_pixel(15, 15, 0xff0fff00);
  draw_pixel(17, 15, 0xff0fff00);
  draw_pixel(15, 16, 0xff0fff00);
  draw_pixel(17, 16, 0xff0fff00);

  RSDP_t* rsdp = find_rsdp();
  if (rsdp == nullptr) {
    // we're fucked lol
    kernel_panic("no rsdt found =(");
  }

  FADT_t* t = find_table(rsdp, "FACP");

  if (t != nullptr) {
    println("whoopy doopy");
    draw_pixel(5, 5, 0xff00ff00);
    draw_pixel(7, 5, 0xff00ff00);
    draw_pixel(5, 6, 0xff00ff00);
    draw_pixel(7, 6, 0xff00ff00);
  }

  MCFG_t* mcfg = find_table(rsdp, "MCFG");

  register_pci_bridges_from_mcfg((uintptr_t)mcfg);

  enumerate_bridges(enumerate_bus);

}

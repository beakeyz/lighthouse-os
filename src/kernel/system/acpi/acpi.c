#include "acpi.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>

void init_acpi() {

  init_acpi_parser();

  RSDP_t* rsdp = find_rsdp();
  if (rsdp == nullptr) {
    // we're fucked lol
    println("no rsdt found =(");
  }

  FADT_t* t = find_table(rsdp, "FACP");

  if (t != nullptr) {
    draw_pixel(5, 5, 0xff00ff00);
    draw_pixel(7, 5, 0xff00ff00);
    draw_pixel(5, 6, 0xff00ff00);
    draw_pixel(7, 6, 0xff00ff00);
  }

  println("whoopy doopy");
  println(to_string(rsdp->revision));

}

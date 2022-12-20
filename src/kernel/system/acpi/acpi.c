#include "acpi.h"
#include "dev/debug/serial.h"
#include "system/acpi/parser.h"

void init_acpi() {

  init_acpi_parser();

  void* rsdt = find_rsdt();
  if (rsdt == nullptr) {
    // we're fucked lol
    println("no rsdt found =(");
  }
  println("whoopy doopy");

}

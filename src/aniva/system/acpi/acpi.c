#include "acpi.h"
#include "dev/debug/serial.h"
#include "dev/loader.h"
#include "libk/flow/error.h"
#include "proc/profile/profile.h"
#include "system/acpi/parser.h"
#include <libk/stddef.h>
#include <libk/string.h>

static acpi_parser_t _parser;

/*!
 * @brief: Initialize the systems ACPI thingy
 *
 * First we create a static in-house ACPI table parser
 * Then we initialize our ACPICA subsystem
 */
void init_acpi() 
{
  init_acpi_parser_early(&_parser);

  /* Init acpica */
  init_acpi_early();

  //kernel_panic(")/ o.o)/ : How the fuck did we exit early ACPI init?");

  Must(init_acpi_parser(&_parser));

  /* Init devices through ACPI */
  init_acpi_devices();

  // TODO: check FADT table for irq shit and verify that the parser booted up nicely
  // TODO: the purpose of this stub is to verify what the parser did while booting 
  // and making sure the rest of the boot happens correctly, based on what the parser
  // is able to find and use. For example, if it does not find an MADT, we should inform
  // the kernel and boot with legacy PIC
}

void init_acpi_core()
{
  ASSERT(load_external_driver_from_var(ACPICORE_DRV_VAR_PATH));
}

void get_root_acpi_parser(struct acpi_parser** out)
{
  if (!out)
    return;

  *out = &_parser;
}

paddr_t find_acpi_root_ptr()
{
  if (_parser.m_is_xsdp)
    return _parser.m_xsdp_phys;

  return _parser.m_rsdp_phys;
}

void* find_acpi_table(char* signature, size_t table_size)
{
  return acpi_parser_find_table(&_parser, signature, table_size);
}

#include "acpi.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
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
  Must(create_acpi_parser(&_parser));

  /* Init acpica */
  //init_acpi_early();

  //kernel_panic(")/ o.o)/ : How the fuck did we exit early ACPI init?");

  // TODO: check FADT table for irq shit and verify that the parser booted up nicely
  // TODO: the purpose of this stub is to verify what the parser did while booting 
  // and making sure the rest of the boot happens correctly, based on what the parser
  // is able to find and use. For example, if it does not find an MADT, we should inform
  // the kernel and boot with legacy PIC
}

void get_root_acpi_parser(struct acpi_parser** out)
{
  if (!out)
    return;

  *out = &_parser;
}

uintptr_t find_acpi_root_ptr()
{
  if (_parser.m_xsdp)
    return _parser.m_xsdp->xsdt_addr;

  return _parser.m_rsdp->rsdt_addr;
}

void* find_acpi_table(char* signature, size_t table_size)
{
  return acpi_parser_find_table(&_parser, signature, table_size);
}

#include "util.h"
#include "drivers/util/kterm/kterm.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"

static const char* __help_str = 
"Welcome to the Aniva kernel terminal application (kterm)\n"
"kterm provides a few internal utilities and a way to execute binaries from the filesystem\n"
"\nA few commands available directly from kterm are: \n"
" - help: display this helpful information\n"
" - exit: exit the kterm driver and (in most cases) exit and shutdown the sytem\n"
" - clear: clear the screen\n"
" - sysinfo: dump some interesting system information\n"
" - ... (TODO: more)";

uint32_t kterm_cmd_help(const char** argv, size_t argc)
{
  kterm_println(__help_str);
  return 0;
}

uint32_t kterm_cmd_exit(const char** argv, size_t argc)
{
  kernel_panic("TODO: how to exit the system?");
  return 0;
}

uint32_t kterm_cmd_clear(const char** argv, size_t argc)
{
  kterm_clear();
  return 0;
}

static ALWAYS_INLINE void kterm_print_keyvalue(const char* key, const char* value)
{
  kterm_print(key);
  kterm_print(": ");
  kterm_println(value);
}

uint32_t kterm_cmd_sysinfo(const char** argv, size_t argc)
{
  acpi_parser_t* parser;

  kterm_println(nullptr);

  kterm_print_keyvalue("System name", "Aniva");
  kterm_print_keyvalue("System version", to_string(kernel_version.version));

  get_root_acpi_parser(&parser);

  if (parser) {
    kterm_print_keyvalue("ACPI table count", to_string(parser->m_tables->m_length));

    kterm_print_keyvalue("ACPI rsdp paddr", to_string(
          parser->m_rsdp ? kmem_to_phys(nullptr, (uint64_t)parser->m_rsdp) : 0));
    kterm_print_keyvalue("ACPI xsdp paddr", to_string(
          parser->m_xsdp ? kmem_to_phys(nullptr, (uint64_t)parser->m_xsdp) : 0));

    kterm_print_keyvalue("ACPI revision", to_string((uint64_t)parser->m_acpi_rev));
    kterm_print_keyvalue("ACPI method", parser->m_rsdp_discovery_method.m_name);
  }

  return 0;
}

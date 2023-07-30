#include "ksyms.h"
#include "dev/debug/serial.h"
#include <libk/string.h>

extern char* _start_ksyms[];
extern char* _end_ksyms[];

extern size_t __ksyms_count;
extern uint8_t __ksyms_table;

const char* get_ksym_name(uintptr_t address)
{
  uint8_t* i = &__ksyms_table;
  ksym_t* current_symbol = (ksym_t*)i;

  while (current_symbol->sym_len) {
    current_symbol = (ksym_t*)i;

    if (current_symbol->address == address)
      return current_symbol->name;
    
    i += current_symbol->sym_len;
  }

  return nullptr;
}

uintptr_t get_ksym_address(char* name)
{
  uint8_t* i = &__ksyms_table;
  ksym_t* current_symbol = (ksym_t*)i;

  if (!name || !*name)
    return NULL;

  while (current_symbol->sym_len) {
    current_symbol = (ksym_t*)i;

    if (memcmp(name, current_symbol->name, strlen(current_symbol->name)) &&
        memcmp(name, current_symbol->name, strlen(name)))
      return current_symbol->address;
    
    i += current_symbol->sym_len;
  }

  return NULL;
}

size_t get_total_ksym_area_size()
{
  return (_end_ksyms - _start_ksyms);
}

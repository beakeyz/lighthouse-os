#ifndef __ANIVA_KSYMS__
#define __ANIVA_KSYMS__

#include <libk/stddef.h>

typedef struct ksym {
  /* Total length of the symbol */
  size_t sym_len;
  /* Virtual address inside the kernel */
  vaddr_t address;
  const char name[];
} ksym_t;

size_t get_total_ksym_area_size();
const char* get_ksym_name(uintptr_t address);
const char* get_best_ksym_name(uintptr_t address);
uintptr_t get_ksym_address(char* name);

#define FOREACH_KSYM()

#endif

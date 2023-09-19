#ifndef __ANIVA_KTERM_EXEC__
#define __ANIVA_KTERM_EXEC__

/*
 * Utility extention of the aniva kterm to execute elf binaries from a cli
 */

#include "kterm.h"

void kterm_try_exec(char* buffer, uint32_t buffer_size);

#endif // !__ANIVA_KTERM_EXEC__

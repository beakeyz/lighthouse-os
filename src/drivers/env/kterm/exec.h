#ifndef __ANIVA_KTERM_EXEC__
#define __ANIVA_KTERM_EXEC__

/*
 * Utility extention of the aniva kterm to execute elf binaries from a cli
 */

#include <libk/stddef.h>

uint32_t kterm_try_exec(const char** argv, size_t argc, const char* cmdline);

#endif // !__ANIVA_KTERM_EXEC__

#ifndef __ANIVA_LIBK_ELF_BIN__
#define __ANIVA_LIBK_ELF_BIN__

#include "fs/file.h"
#include "libk/error.h"
#include "proc/proc.h"
#include <libk/stddef.h>

/*
 * Create a process that starts execution at the entrypoint of the elf file
 */
proc_t* elf_exec_static_64(file_t* file, bool kernel);
proc_t* elf_exec_static_32(file_t* file, bool kernel);

#endif // !__ANIVA_LIBK_ELF_BIN__

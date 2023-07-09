#ifndef __ANIVA_LIBK_ELF_BIN__
#define __ANIVA_LIBK_ELF_BIN__

#include "fs/file.h"
#include "libk/flow/error.h"
#include "proc/proc.h"
#include <libk/stddef.h>

/*
 * Create a process that starts execution at the entrypoint of the elf file
 * and imidiately yield to it
 */
ErrorOrPtr elf_exec_static_64_ex(file_t* file, bool kernel, bool defer_schedule);
ErrorOrPtr elf_exec_static_32_ex(file_t* file, bool kernel, bool defer_schedule);

#endif // !__ANIVA_LIBK_ELF_BIN__

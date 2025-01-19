#ifndef __ANIVA_KTERM_FS__
#define __ANIVA_KTERM_FS__

#include "drivers/env/kterm/kterm.h"
#include <libk/stddef.h>

void kterm_init_fs(kterm_login_t* login);

uint32_t kterm_fs_print_working_dir();
u32 kterm_fs_selo(const char** argv, u32 argc);

#endif // !__ANIVA_KTERM_FS__

#ifndef __ANIVA_KTERM_FS__
#define __ANIVA_KTERM_FS__

#include "drivers/util/kterm/kterm.h"
#include <libk/stddef.h>

void kterm_init_fs(kterm_login_t* login);

uint32_t kterm_fs_print_working_dir();

#endif // !__ANIVA_KTERM_FS__

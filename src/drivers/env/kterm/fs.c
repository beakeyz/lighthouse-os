#include "fs.h"
#include "drivers/env/kterm/kterm.h"

static kterm_login_t* _login;

void kterm_init_fs(kterm_login_t* login)
{
    _login = login;
}

uint32_t kterm_fs_print_working_dir()
{
    kterm_println(_login->cwd);
    return 0;
}

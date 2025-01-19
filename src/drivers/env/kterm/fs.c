#include "fs.h"
#include "drivers/env/kterm/kterm.h"
#include "oss/core.h"
#include "oss/object.h"

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

u32 kterm_fs_selo(const char** argv, u32 argc)
{
    i32 error;
    oss_object_t* new_object;
    const char* path;

    if (argc != 2)
        return 0;

    path = argv[1];

    /* Resolve a relative path */
    error = oss_open_object_from(path, _login->c_obj, &new_object);

    /* Try again, but now absolute */
    if (error)
        error = oss_open_object(path, &new_object);

    /* Invalid path xD */
    if (error)
        return error;

    return kterm_set_cwd(NULL, new_object);
}

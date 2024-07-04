#include "fs.h"
#include "drivers/env/kterm/kterm.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"

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

u32 kterm_fs_cd(const char** argv, u32 argc)
{
    i32 error;
    oss_node_t* new_node;
    const char* path;

    if (argc != 2)
        return 0;

    path = argv[1];

    /* Resolve a relative path */
    error = oss_resolve_node_rel(_login->c_node, path, &new_node);

    /* Try again, but now absolute */
    if (error)
        error = oss_resolve_node(path, &new_node);

    /* Invalid path xD */
    if (error)
        return error;

    kfree((void*)_login->cwd);

    return kterm_set_cwd(NULL, new_node);
}

#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/proc/cmdline.h"
#include <lightos/handle.h>
#include <lightos/object.h>
#include <stdio.h>

/* Holds the relative path where we want to search from */
const char* __relative_path = NULL;

static void __parse_flag(const char* flag_arg)
{
}

static error_t __parse_cmdline(u32 argc)
{
    const char* arg;

    if (!argc)
        return 0;

    for (u32 i = 1; i < argc; i++) {
        arg = cmdline_get_arg(i);

        if (!arg)
            continue;

        if (arg[0] == '-') {
            __parse_flag(arg);
            continue;
        }

        if (!__relative_path)
            __relative_path = arg;
    }

    return 0;
}

static Object* __get_relative_object(Object* self_obj)
{
    char path_buff[256] = { 0 };

    if (__relative_path)
        return OpenObject(__relative_path, HNDL_FLAG_R, NULL);

    /* Try to open the current directory sysvar object */
    Object* cwd = OpenObjectFrom(self_obj, "CWD", HNDL_FLAG_R, NULL);

    if (!ObjectIsValid(cwd))
        return cwd;

    if (ObjectRead(cwd, 0, path_buff, sizeof(path_buff)))
        return nullptr;

    /* Close the object */
    CloseObject(cwd);

    /* Try to open the object pointed to by CWD */
    return OpenObject(path_buff, HNDL_FLAG_R, NULL);
}

error_t main(HANDLE self)
{
    error_t error;
    /* Grab the commandline argument count */
    u32 argc = cmdline_get_argc();
    u32 idx = 0;

    Object* rel_obj;
    Object self_obj = {
        .handle = self,
        .type = OT_PROCESS,
    };

    /* Try to parse the command line (Shouldn't fail lol) */
    error = __parse_cmdline(argc);

    if (error)
        return error;

    rel_obj = __get_relative_object(&self_obj);

    if (!ObjectIsValid(rel_obj))
        return -ENOENT;

    while (true) {
        /* Grab this guy */
        Object* walker = OpenObjectFromIdx(rel_obj, idx, HNDL_FLAG_R, NULL);

        if (!ObjectIsValid(walker))
            break;

        printf(" %s%s", walker->key, (walker->type == OT_DIR || walker->type == OT_DGROUP || walker->type == OT_GENERIC) ? "/\n" : "\n");

        /* Also make sure to close the object */
        CloseObject(walker);

        idx++;
    }

    return 0;
}

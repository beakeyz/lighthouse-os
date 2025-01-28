#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/object.h"
#include "lightos/proc/cmdline.h"

#include <stdio.h>

static const char* __target_path;

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

        if (!__target_path)
            __target_path = arg;
    }

    return 0;
}

static Object* __get_target_object(Object* self_obj)
{
    Object *relative, *ret;

    /* Try to open the object pointed to by CWD */
    relative = OpenWorkingObject(HF_R);

    if (!relative)
        return nullptr;

    ret = OpenObjectFrom(relative, __target_path, HF_R, NULL);

    CloseObject(relative);

    return ret;
}

static int dump_object(Object* object)
{
    ssize_t sz_or_error;
    u64 offset = 0;

    while (true) {
        char buf[0x1000] = { 0 };

        sz_or_error = ObjectRead(object, offset, buf, sizeof(buf));

        if (IS_FATAL(sz_or_error))
            break;

        sz_or_error = ObjectWrite(stdout->object, offset, buf, sz_or_error);

        if (IS_FATAL(sz_or_error))
            break;

        offset += sizeof(buf);
    }

    return sz_or_error;
}

int main(HANDLE self)
{
    Object *target, *self_obj;
    u32 argc = cmdline_get_argc();

    /* Parse commandline arguments */
    __parse_cmdline(argc);

    /* Create a handle from our process handle */
    self_obj = GetObjectFromHandle(self);

    target = __get_target_object(self_obj);

    /* Could not find a target =( */
    if (!target)
        return -ENOENT;

    /* Just dump the object without complaining */
    (void)dump_object(target);

    CloseObject(target);

    return 0;
}

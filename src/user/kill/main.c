
#include "errno.h"
#include "lightos/handle_def.h"
#include "lightos/proc/cmdline.h"
#include "lightos/proc/process.h"
#include "lightos/proc/shared.h"
#include "sys/types.h"
#include <lightos/handle.h>

static CMDLINE cmdline;
static const char* target_path;
static bool force;

static void __parse_args()
{
    const char* c_arg;

    target_path = nullptr;
    force = false;

    for (int i = 1; i < cmdline.argc; i++) {
        c_arg = cmdline.argv[i];

        if (!c_arg)
            continue;

        if (*c_arg != '-' && !target_path)
            target_path = c_arg;

        switch (c_arg[1]) {
        /* Force this killing */
        case 'f':
            force = true;
            break;
        default:
            break;
        }
    }
}

/*!
 * @brief: The kill process
 *
 * Tries to murder processes
 *
 * TODO: Support multiple process killings with a single command (Mass murder)
 */
int main()
{
    int error;
    HANDLE process_handle;

    error = cmdline_get(&cmdline);

    if (error)
        return error;

    __parse_args();

    process_handle = open_proc(target_path, NULL, NULL);

    if (handle_verify(process_handle))
        return -EBADHANDLE;

    /* Failing to close is most likely a permission issue */
    if (!kill_process(process_handle, force ? LIGHTOS_PROC_FLAG_FORCE : NULL))
        return -EPERM;

    return 0;
}

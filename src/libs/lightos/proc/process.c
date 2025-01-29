
#include "process.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/handle.h"
#include "lightos/syscall.h"

HANDLE open_proc(const char* name, u32 flags, u32 mode)
{
    return open_handle(name, flags, OT_PROCESS, mode);
}

HANDLE create_process(const char* name, FuncPtr entry, const char** args, size_t argc, u32 flags)
{
}

error_t process_kill(HANDLE handle, u32 flags)
{
}

extern size_t get_process_time(HANDLE handle)
{
    return sys_get_process_time();
}

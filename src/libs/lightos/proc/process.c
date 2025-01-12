
#include "process.h"
#include "lightos/handle.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"

HANDLE open_proc(const char* name, u32 flags, u32 mode)
{
    return open_handle(name, HNDL_TYPE_PROC, flags, mode);
}

BOOL proc_send_data(HANDLE handle, control_code_t code, VOID* buffer, size_t* buffer_size, u32 flags)
{
    /* TODO: */
    return FALSE;
}

BOOL proc_get_profile(HANDLE proc_handle, HANDLE* profile_handle)
{
    /* TODO: make a way for us to discover the name of this process through its handle */
    return FALSE;
}

BOOL create_process(const char* name, FuncPtr entry, const char** args, size_t argc, u32 flags)
{
    uintptr_t sys_result;

    /* Cry to the kernel about it */
    sys_result = sys_create_proc(name, entry);

    if (sys_result == 0)
        return TRUE;

    return FALSE;
}

BOOL kill_process(HANDLE handle, u32 flags)
{
    uintptr_t sys_result;

    /* Cry to the kernel about it */
    sys_result = sys_destroy_proc(handle, flags);

    if (sys_result == 0)
        return TRUE;

    return FALSE;
}

size_t get_process_time()
{
    return sys_get_process_time();
}

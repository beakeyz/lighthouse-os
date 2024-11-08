
#include "process.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"

HANDLE open_proc(const char* name, DWORD flags, DWORD mode)
{
    return open_handle(name, HNDL_TYPE_PROC, flags, mode);
}

BOOL proc_send_data(HANDLE handle, control_code_t code, VOID* buffer, size_t* buffer_size, DWORD flags)
{
    /* TODO: */
    return FALSE;
}

BOOL proc_get_profile(HANDLE proc_handle, HANDLE* profile_handle)
{
    /* TODO: make a way for us to discover the name of this process through its handle */
    return FALSE;
}

BOOL create_process(const char* name, FuncPtr entry, const char** args, size_t argc, DWORD flags)
{
    uintptr_t sys_result;

    /* Cry to the kernel about it */
    sys_result = syscall_2(SYSID_CREATE_PROC, (uintptr_t)name, (uintptr_t)entry);

    if (sys_result == SYS_OK)
        return TRUE;

    return FALSE;
}

BOOL kill_process(HANDLE handle, DWORD flags)
{
    uintptr_t sys_result;

    /* Cry to the kernel about it */
    sys_result = syscall_2(SYSID_DESTROY_PROC, handle, flags);

    if (sys_result == SYS_OK)
        return TRUE;

    return FALSE;
}

size_t get_process_time()
{
    return syscall_0(SYSID_GET_PROCESSTIME);
}

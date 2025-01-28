#include "lib.h"
#include "lightos/handle.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"

/*
 * TODO: We need to create integration between this and the dynamic loader
 * driver
 */

BOOL lib_load(const char* path, HANDLE* out_handle)
{
    HANDLE hdl;

    if (!path || !out_handle)
        return FALSE;

    hdl = open_handle(path, HNDL_TYPE_SHARED_LIB, HF_RW, NULL);

    if (handle_verify(hdl))
        return FALSE;

    *out_handle = hdl;
    return TRUE;
}

BOOL lib_unload(HANDLE handle)
{
    return close_handle(handle);
}

BOOL lib_open(const char* path, HANDLE* phandle)
{
    exit_noimpl("lib_open");
    return FALSE;
}

BOOL lib_get_function(HANDLE lib_handle, const char* func, VOID** faddr)
{
    VOID* addr;

    if (!faddr)
        return FALSE;

    *faddr = NULL;

    /* Call the kernel =) */
    addr = sys_get_function(lib_handle, func);

    if (!addr)
        return FALSE;

    *faddr = (void*)addr;
    return true;
}

VOID* lib_get_load_addr(HANDLE lib)
{
    exit_noimpl("lib_get_load_addr");
    return NULL;
}

u64 lib_get_load_size(HANDLE lib)
{
    exit_noimpl("lib_get_load_size");
    return NULL;
}

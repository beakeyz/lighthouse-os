#ifndef __ANIVA_LIBENV_DYNAMIC__
#define __ANIVA_LIBENV_DYNAMIC__

#include "handle_def.h"
#include "lightos/system.h"
#include "sys/types.h"

extern BOOL load_library(
    __IN__ const char* path,
    __OUT__ HANDLE* out_handle);

extern BOOL unload_library(
    __IN__ HANDLE handle);

extern BOOL get_func_address(
    __IN__ HANDLE lib_handle,
    __IN__ const char* func,
    __OUT__ VOID** faddr);

#endif // !__ANIVA_LIBENV_DYNAMIC__

#ifndef __ANIVA_LIBENV_DYNAMIC__
#define __ANIVA_LIBENV_DYNAMIC__

#include "handle_def.h"
#include "lightos/system.h"
#include "sys/types.h"

extern BOOL lib_load(
     const char* path,
     HANDLE* out_handle);

extern BOOL lib_unload(
     HANDLE handle);

extern BOOL lib_get_function(
     HANDLE lib_handle,
     const char* func,
     VOID** faddr);

#endif // !__ANIVA_LIBENV_DYNAMIC__

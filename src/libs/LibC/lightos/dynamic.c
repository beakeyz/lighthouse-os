#include "dynamic.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include "stdlib.h"

/*
 * TODO: We need to create integration between this and the dynamic loader 
 * driver
 */

BOOL load_library(const char* path, HANDLE* out_handle)
{
  HANDLE hdl;

  if (!path || !out_handle)
    return FALSE;

  hdl = open_handle(path, HNDL_TYPE_SHARED_LIB, HNDL_FLAG_RW, NULL);

  if (!handle_verify(hdl))
    return FALSE;

  *out_handle = hdl;
  return TRUE;
}

BOOL unload_library(HANDLE handle)
{
  return close_handle(handle);
}

BOOL get_func_address(HANDLE lib_handle, const char* func, VOID** faddr)
{
  uintptr_t addr;

  if (!faddr)
    return FALSE;

  *faddr = NULL;

  /* Call the kernel =) */
  addr = syscall_2(SYSID_GET_FUNCADDR, lib_handle, (uintptr_t)func);

  if (!addr)
    return FALSE;

  *faddr = (void*)addr;
  return true;
}

#include "dynamic.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
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

BOOL get_func_address(HANDLE lib_handle, VOID** faddr)
{
  exit_noimpl("get_func_address");
  return FALSE;
}

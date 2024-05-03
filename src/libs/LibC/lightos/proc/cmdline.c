#include "cmdline.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/var/var.h"
#include <stdlib.h>
#include <string.h>

lightos_cmdline_t __this_cmdline;

int __init_lightos_cmdline()
{
  HANDLE env_handle;
  HANDLE cmdline_var;

  memset(&__this_cmdline, 0, sizeof(__this_cmdline));

  /* Get the current environment */
  env_handle = open_handle(NULL, HNDL_TYPE_PROC_ENV, NULL, HNDL_MODE_CURRENT_ENV);

  /* Yikes */
  if (!handle_verify(env_handle))
    return 0;

  cmdline_var = open_sysvar(env_handle, CMDLINE_VARNAME, HNDL_FLAG_R);

  /* Yikes^2 */
  if (!handle_verify(cmdline_var))
    return 0;

  __this_cmdline.raw = malloc(CMDLINE_MAX_LEN);

  /* Bruhhhh */
  if (!__this_cmdline.raw)
    return 0;

  memset((void*)__this_cmdline.raw, 0, CMDLINE_MAX_LEN);

  if (!sysvar_read(cmdline_var, CMDLINE_MAX_LEN, (void*)__this_cmdline.raw))
    return 0;

close_handles_and_exit:
  close_handle(cmdline_var);
close_env_handle_and_exit:
  close_handle(env_handle);
  return 0;
}

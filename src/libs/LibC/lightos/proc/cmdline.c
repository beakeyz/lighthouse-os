#include "cmdline.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/var/var.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

lightos_cmdline_t __this_cmdline;

int cmdline_get_len(size_t* bsize)
{
  if (!bsize)
    return -1;

  *bsize = NULL;

  if (!__this_cmdline.raw)
    return -1;

  *bsize = strlen(__this_cmdline.raw);
  return 0;
}

int cmdline_get_raw(char* buffer, size_t bsize)
{
  return 0;
}

int cmdline_get(CMDLINE* b_cmdline)
{
  if (!__this_cmdline.raw)
    return -1;

  b_cmdline->raw = strdup(__this_cmdline.raw);
  b_cmdline->argv = malloc(sizeof(char*) * __this_cmdline.argc);
  b_cmdline->argc = __this_cmdline.argc;

  for (uint32_t i = 0; i < b_cmdline->argc; i++)
    b_cmdline->argv[i] = __this_cmdline.argv[i];

  return 0;
}

void cmdline_destroy(CMDLINE* cmdline)
{
  free((void*)cmdline->raw);

  for (uint32_t i = 0; i < cmdline->argc; i++)
    free(cmdline->argv[i]);
  free(cmdline->argv);
}

/*!
 * @brief: Fetch the commandline from the current processes environment
 *
 */
int __get_raw_cmdline()
{
  int error;
  HANDLE cmdline_var;

  error = -1;
  memset(&__this_cmdline, 0, sizeof(__this_cmdline));

  cmdline_var = open_sysvar(CMDLINE_VARNAME, HNDL_FLAG_R);

  /* Yikes^2 */
  if (!handle_verify(cmdline_var))
    return error;

  __this_cmdline.raw = malloc(CMDLINE_MAX_LEN);

  /* Bruhhhh */
  if (!__this_cmdline.raw)
    goto close_handles_and_exit;

  (void)memset((void*)__this_cmdline.raw, 0, CMDLINE_MAX_LEN);
  (void)sysvar_read(cmdline_var, CMDLINE_MAX_LEN, (void*)__this_cmdline.raw);

  error = 0;

close_handles_and_exit:
  close_handle(cmdline_var);
  return error;
}

/*!
 * @brief: Parse the raw commandline
 *
 * Turns the commandline into an argv and argc
 */
int __parse_cmdline()
{

  return 0;
}

int __init_lightos_cmdline()
{
  if (__get_raw_cmdline())
    return 0;

  return __parse_cmdline();
}

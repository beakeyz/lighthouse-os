#ifndef __LIGHTOS_PROC_CMDLINE__
#define __LIGHTOS_PROC_CMDLINE__

/*
 * Header which outlines the cmdline stuff for processes
 * every environment may have a commandline passed with it, which will get
 * put into the CMDLINE variable on the local environment.
 *
 * NOTE: These functions are implemented in cmdline.c
 */

#include <stdint.h>

#define CMDLINE_VARNAME "CMDLINE"
#define CMDLINE_MAX_LEN 1024

/*
 * Top most structure of the commandline
 */
typedef struct lightos_cmdline {
  const char* raw;
  char** argv;
  uint32_t argc;
} lightos_cmdline_t, CMDLINE;

extern int __init_lightos_cmdline();

#endif // !__LIGHTOS_PROC_CMDLINE__

#ifndef __ANIVA_KTERM_HELP__
#define __ANIVA_KTERM_HELP__

#include <libk/stddef.h>

/*
 * Utility extention of the aniva kterm to support advanced help dialouge for the user
 */

static inline bool cmd_has_args(size_t argc)
{
  return (argc > 1);
}

uint32_t kterm_cmd_help(const char** argv, size_t argc);
uint32_t kterm_cmd_exit(const char** argv, size_t argc);
uint32_t kterm_cmd_clear(const char** argv, size_t argc);

uint32_t kterm_cmd_sysinfo(const char** argv, size_t argc);
uint32_t kterm_cmd_drvinfo(const char** argv, size_t argc);

uint32_t kterm_cmd_hello(const char** argv, size_t argc);
uint32_t kterm_cmd_drvld(const char** argv, size_t argc);
uint32_t kterm_cmd_diskinfo(const char** argv, size_t argc);
uint32_t kterm_cmd_procinfo(const char** argv, size_t argc);

extern uint32_t kterm_cmd_envinfo(const char** argv, size_t argc);

#endif // !__ANIVA_KTERM_HELP__

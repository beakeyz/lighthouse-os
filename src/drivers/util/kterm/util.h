#ifndef __ANIVA_KTERM_HELP__
#define __ANIVA_KTERM_HELP__

#include <libk/stddef.h>

/*
 * Utility extention of the aniva kterm to support advanced help dialouge for the user
 */

uint32_t kterm_cmd_help(const char** argv, size_t argc);
uint32_t kterm_cmd_exit(const char** argv, size_t argc);
uint32_t kterm_cmd_clear(const char** argv, size_t argc);

uint32_t kterm_cmd_sysinfo(const char** argv, size_t argc);

#endif // !__ANIVA_KTERM_HELP__

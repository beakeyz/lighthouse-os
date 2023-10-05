#ifndef __ANIVA_KTERM_DRIVER__
#define __ANIVA_KTERM_DRIVER__

#include "dev/driver.h"

#define KTERM_DRV_DRAW_STRING   10
#define KTERM_DRV_MAP_FB        11
#define KTERM_DRV_CLEAR         12

/*
 * Generic kterm commands 
 * 
 * NOTE: every command handler should retern POSITIVE error codes
 */
typedef uint32_t (*f_kterm_command_handler_t) (const char** argv, size_t argc);

#endif // !__ANIVA_KTERM_DRIVER__

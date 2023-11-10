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

/*
 * I expose kterm_println here for internal driver useage (to ensure that 
 * logs from inside the driver dont get stuck somewhere else inside the loggin 
 * system) since the rest of the kernel won't be able to access this directly any-
 * ways because the kernel never gets linked with this. The driver loader inside
 * the kernel only links the driver to the kernel symbols, so we are safe to expose
 * driver functions in driver headers =)
 */
int kterm_println(const char* msg);
int kterm_print(const char* msg);

void kterm_clear();

struct kterm_cmd {
  char* argv_zero;
  char* desc;
  f_kterm_command_handler_t handler;
};

extern struct kterm_cmd kterm_commands[];
extern uint32_t kterm_cmd_count;

#endif // !__ANIVA_KTERM_DRIVER__

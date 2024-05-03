#ifndef __ANIVA_KTERM_DRIVER__
#define __ANIVA_KTERM_DRIVER__

#include <libk/stddef.h>

#define KTERM_DRV_DRAW_STRING   7
#define KTERM_DRV_MAP_FB        8
#define KTERM_DRV_CLEAR         9

struct user_profile;

enum kterm_mode {
  KTERM_MODE_LOADING = 0,
  KTERM_MODE_TERMINAL,
  KTERM_MODE_GRAPHICS,
};

typedef struct kterm_login {
  struct user_profile* profile;
  char* cwd;
} kterm_login_t;

/*
 * Generic kterm commands 
 * 
 * NOTE: every command handler should retern POSITIVE error codes
 */
typedef uint32_t (*f_kterm_command_handler_t) (const char** argv, size_t argc, const char* cmdline);

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
int kterm_putc(char msg);

void kterm_clear();
void kterm_switch_to_terminal();

bool kterm_is_logged_in();
int kterm_set_login(struct user_profile* profile);
int kterm_get_login(struct user_profile** profile);

bool kterm_ismode(enum kterm_mode mode);

struct kterm_cmd {
  char* argv_zero;
  char* desc;
  f_kterm_command_handler_t handler;
};

int kterm_create_prompt(const char* prompt, char* buffer, size_t buffersize, bool hide_input);

extern int kterm_handle_login();

extern struct kterm_cmd kterm_commands[];
extern uint32_t kterm_cmd_count;

#endif // !__ANIVA_KTERM_DRIVER__

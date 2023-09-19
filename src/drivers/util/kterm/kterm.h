#ifndef __ANIVA_KTERM_DRIVER__
#define __ANIVA_KTERM_DRIVER__

#include "dev/driver.h"

#define KTERM_DRV_DRAW_STRING   10
#define KTERM_DRV_MAP_FB        11
#define KTERM_DRV_CLEAR         12

typedef struct kterm_cmd {
  char* title;
  int (*f_exec)(uint8_t* buffer, uint32_t buffer_size);
} kterm_cmd_t;

//void println_kterm(const char*);

#endif // !__ANIVA_KTERM_DRIVER__

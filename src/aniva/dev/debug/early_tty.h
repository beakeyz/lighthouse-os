#ifndef __ANIVA_EARLY_TTY__
#define __ANIVA_EARLY_TTY__

#include "libk/multiboot.h"

void init_early_tty(struct multiboot_tag_framebuffer* fb);
void destroy_early_tty();

void etty_print(char* str);
void etty_println(char* str);

#endif // !__ANIVA_EARLY_TTY__

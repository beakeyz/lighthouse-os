#ifndef __ANIVA_LWND_ALLOC__
#define __ANIVA_LWND_ALLOC__

#include "drivers/lwnd/screen.h"
#include "drivers/lwnd/window.h"

int init_lwnd_alloc();

lwnd_window_t* allocate_lwnd_window();
void deallocate_lwnd_window(lwnd_window_t* window);

lwnd_screen_t* allocate_lwnd_screen();
void deallocate_lwnd_screen(lwnd_screen_t* screen);

#endif // !__ANIVA_LWND_ALLOC__

#ifndef __LWND_WINDOWING_STACK__
#define __LWND_WINDOWING_STACK__

#include "libk/data/hashmap.h"
#include "libk/stddef.h"

struct lwnd_window;

#define LWND_WNDSTACK_DEFAULT_MAXWND 1024

typedef struct lwnd_wndstack {
    u16 max_n_wnd;

    /* Windows are stored based on their names */
    hashmap_t* wnd_map;

    /* The top window */
    struct lwnd_window* top_window;
    struct lwnd_window* bottom_window;
} lwnd_wndstack_t;

lwnd_wndstack_t* create_lwnd_wndstack(u16 max_n_wnd);
void destroy_lwnd_wndstack(lwnd_wndstack_t* stack);

int wndstack_add_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);
int wndstack_remove_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);

int wndstack_focus_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);

#endif // !__LWND_WINDOWING_STACK__

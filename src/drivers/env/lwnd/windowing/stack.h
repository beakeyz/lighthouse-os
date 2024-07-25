#ifndef __LWND_WINDOWING_STACK__
#define __LWND_WINDOWING_STACK__

#include "libk/data/hashmap.h"
#include "libk/stddef.h"

struct lwnd_window;
struct lwnd_screen;

#define LWND_WNDSTACK_DEFAULT_MAXWND 1024

typedef struct lwnd_wndstack {
    u16 max_n_wnd;

    /* Windows are stored based on their names */
    hashmap_t* wnd_map;

    /*
     * Reference to the screen this stack is bound to.
     * When a stack is moved from one screen to another, there are a
     * lot of things that need managing, so don't just change
     * this value whenever plz
     */
    struct lwnd_screen* screen;

    /* The top window */
    struct lwnd_window* top_window;
    struct lwnd_window* bottom_window;

    /* Not a real 'window', but just used as a dummy */
    struct lwnd_window* background_window;
} lwnd_wndstack_t;

lwnd_wndstack_t* create_lwnd_wndstack(u16 max_n_wnd, struct lwnd_screen* screen);
void destroy_lwnd_wndstack(lwnd_wndstack_t* stack);

int lwnd_wndstack_update_background(lwnd_wndstack_t* stack);

struct lwnd_window* wndstack_find_window(lwnd_wndstack_t* stack, const char* title);

int wndstack_add_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);
int wndstack_remove_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);

int wndstack_focus_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd);
int wndstack_cycle_windows(lwnd_wndstack_t* stack);

#endif // !__LWND_WINDOWING_STACK__

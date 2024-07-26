#ifndef __LWND_WINDOWING_WORKSPACE__
#define __LWND_WINDOWING_WORKSPACE__

#include "drivers/env/lwnd/windowing/stack.h"
#include "proc/proc.h"

#define MAX_WORKSPACES 8

struct lwnd_screen;

typedef struct lwnd_workspace {
    struct lwnd_screen* parent_screen;
    lwnd_wndstack_t* stack;
} lwnd_workspace_t;

int init_lwnd_workspaces(lwnd_workspace_t p_workspaces[], u32* n_workspaces, struct lwnd_screen* screen);
int init_lwnd_workspace(lwnd_workspace_t* ws, struct lwnd_screen* screen);
void destroy_lwnd_workspace(lwnd_workspace_t* ws);

int lwnd_workspace_remove_windows_of_process(lwnd_workspace_t* ws, proc_t* p);

#endif // !__LWND_WINDOWING_WORKSPACE__

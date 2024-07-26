#include "workspace.h"
#include "drivers/env/lwnd/display/screen.h"
#include "drivers/env/lwnd/windowing/stack.h"
#include "libk/flow/error.h"

/*
 * lwnd workspace
 *
 * Manager for the internal window stack. There might be more workspaces per screen,
 * but only one might be visible per screen
 */

/*!
 * @brief: Initialize a fixed array of workspaces
 *
 * Every screen has a maximum of MAX_WORKSPACES workspaces
 */
int init_lwnd_workspaces(lwnd_workspace_t p_workspaces[], u32* n_workspaces, lwnd_screen_t* screen)
{
    int error;

    if (!p_workspaces || !n_workspaces || !screen)
        return -KERR_INVAL;

    if (*n_workspaces > MAX_WORKSPACES)
        *n_workspaces = MAX_WORKSPACES;

    for (u32 i = 0; i < *n_workspaces; i++) {
        error = init_lwnd_workspace(&p_workspaces[i], screen);

        if (error) {
            *n_workspaces = i;
            return error;
        }
    }

    return 0;
}

/*!
 * @brief: Initialize a single workspace
 */
int init_lwnd_workspace(lwnd_workspace_t* ws, lwnd_screen_t* screen)
{
    if (!ws || !screen)
        return -KERR_INVAL;

    ws->parent_screen = screen;
    ws->stack = create_lwnd_wndstack(0xffff, screen);

    return 0;
}

void destroy_lwnd_workspace(lwnd_workspace_t* ws)
{
    destroy_lwnd_wndstack(ws->stack);
}

int lwnd_workspace_remove_windows_of_process(lwnd_workspace_t* ws, proc_t* p)
{
    lwnd_window_t *this_wnd, *next_wnd;

    if (!ws || !p)
        return -KERR_INVAL;

    if (!ws->stack)
        return -KERR_INVAL;

    /* Lock the stack */
    mutex_lock(ws->stack->lock);

    /* Grab the top window */
    this_wnd = ws->stack->top_window;

    /* Move down */
    while (this_wnd) {
        next_wnd = this_wnd->next_layer;

        /* Check */
        if (this_wnd->proc != p)
            goto cycle_next_window;

        if (wndstack_remove_window(ws->stack, this_wnd))
            goto cycle_next_window;

        /* Murder the window */
        destroy_window(this_wnd);
    cycle_next_window:
        this_wnd = next_wnd;
    }

    mutex_unlock(ws->stack->lock);

    return 0;
}

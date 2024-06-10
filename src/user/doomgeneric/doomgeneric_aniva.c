#include "LibGfx/include/driver.h"
#include "LibGfx/include/events.h"
#include "LibGfx/include/lgfx.h"
#include "LibGfx/include/video.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "lightos/event/key.h"
#include "lightos/proc/process.h"

#include <stdio.h>
#include <unistd.h>

#include <stdbool.h>

#include "i_system.h"

lwindow_t window;
lframebuffer_t fb;

/*!
 * @brief: Initialize the game graphics core
 *
 * - Connect to the window server
 * - Setup framebuffer shit
 */
void DG_Init()
{
    BOOL res;

    /* Initialize the graphics API */
    res = request_lwindow(&window, DOOMGENERIC_RESX, DOOMGENERIC_RESY, NULL);

    if (!res)
        I_Error("Could not request window!");

    res = lwindow_request_framebuffer(&window, &fb);

    if (!res)
        I_Error("Could not request framebuffer!");
}

/*!
 * @brief: Copy shit from the back buffer to the lwnd driver
 */
void DG_DrawFrame()
{
    lwindow_draw_buffer(&window, 0, 0, window.current_width, window.current_height, (lcolor_t*)DG_ScreenBuffer);

    /* Get keys from the server */
    get_key_event(&window, NULL);
}

/*!
 * @brief: Sleep until a certain amount of @ms have passed
 */
void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

/*!
 * @brief: Ask our host how many ms have passed since our launch
 */
uint32_t DG_GetTicksMs()
{
    /*
     * TODO: this is currently just in 'ticks' (AKA scheduler ticks)
     * but we'll need a way to convert this into milliseconds
     */
    return get_process_time() * 3;
}

static unsigned char aniva_keycode_to_doomkey(uint32_t keycode)
{
    switch (keycode) {
    case ANIVA_SCANCODE_LCTRL:
    case ANIVA_SCANCODE_RCTRL:
        return KEY_FIRE;
    case ANIVA_SCANCODE_SPACE:
        return KEY_USE;
    case ANIVA_SCANCODE_RSHIFT:
    case ANIVA_SCANCODE_LSHIFT:
        return KEY_RSHIFT;
    case ANIVA_SCANCODE_RALT:
    case ANIVA_SCANCODE_LALT:
        return KEY_LALT;
    case ANIVA_SCANCODE_UP:
        return KEY_UPARROW;
    case ANIVA_SCANCODE_DOWN:
        return KEY_DOWNARROW;
    case ANIVA_SCANCODE_LEFT:
        return KEY_LEFTARROW;
    case ANIVA_SCANCODE_RIGHT:
        return KEY_RIGHTARROW;
    case ANIVA_SCANCODE_ESCAPE:
        return KEY_ESCAPE;

    /* WASD-rebinds */
    case ANIVA_SCANCODE_A:
        return KEY_STRAFE_L;
    case ANIVA_SCANCODE_D:
        return KEY_STRAFE_R;
    case ANIVA_SCANCODE_W:
        return KEY_UPARROW;
    case ANIVA_SCANCODE_S:
        return KEY_DOWNARROW;
    case ANIVA_SCANCODE_Q:
        return KEY_LEFTARROW;
    case ANIVA_SCANCODE_E:
        return KEY_RIGHTARROW;
    case ANIVA_SCANCODE_EQUALS:
        return KEY_EQUALS;
    default:
        return keycode;
    }
}

/*!
 * @brief: Query the system whether there is a pressed key
 */
int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    BOOL has_event;
    lkey_event_t keyevent;

    has_event = get_key_event(&window, &keyevent);

    if (!has_event)
        return 0;

    *pressed = keyevent.pressed;
    *doomKey = aniva_keycode_to_doomkey(keyevent.keycode);

    return 1;
}

void DG_SetWindowTitle(const char* title)
{
}

/* Our own argv and argc vars, since our system does not have those yet =)))) */
char* argv[] = {
    "doom",
    "-testcontrols",
    "-iwad",
    "Root/Apps/doom1.wad",
};
const int argc = 4;

/*!
 * @brief: Main entry for this app
 */
int main(/* int argc, char **argv */)
{
    doomgeneric_Create(argc, argv);

    for (;;) {
        doomgeneric_Tick();
    }

    return 0;
}

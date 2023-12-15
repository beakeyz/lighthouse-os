#include "LibGfx/include/driver.h"
#include "LibGfx/include/events.h"
#include "LibGfx/include/lgfx.h"
#include "LibGfx/include/video.h"
#include "LibSys/event/key.h"
#include "LibSys/proc/process.h"
#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

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
  return get_our_ticks() * 3;
}

static unsigned char aniva_keycode_to_doomkey(uint32_t keycode)
{
  switch (keycode) {
    case ANIVA_SCANCODE_LCTRL:
      return KEY_FIRE;
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

void DG_SetWindowTitle(const char * title)
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

  for (int i = 0; ; i++)
  {
      doomgeneric_Tick();
  }
  
  return 0;
}

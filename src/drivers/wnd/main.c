#include "dev/video/framebuffer.h"
#include "drivers/wnd/io.h"
#include "drivers/wnd/screen.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/video/device.h>
#include "window.h"
#include "LibGfx/include/driver.h"

static fb_info_t _fb_info;
static lwnd_mouse_t _mouse;
static lwnd_keyboard_t _keyboard;

static uint32_t _main_screen;
static vector_t* _screens;

/*!
 * @brief: Main compositing loop
 *
 * A few things need to happen here:
 * 1) Loop over the windows and draw visible stuff
 * 2) Queue events to windows that want them
 */
static void USED lwnd_main()
{
  lwnd_window_t* current_wnd;
  lwnd_screen_t* current_screen;

  (void)_mouse;
  (void)_keyboard;

  while (true) {
    FOREACH_VEC(_screens, data, index) {
      current_screen = *(lwnd_screen_t**)data;

      for (uint64_t i = 0; i < current_screen->window_count; i++) {
        current_wnd = current_screen->window_stack[i];

        /* Check if this windows wants to move */
        /* Clear the old bits in the pixel bitmap (These pixels will be overwritten with whatever is under them)*/
        /* Set the new bits in the pixel bitmap */
        /* Draw whatever is visible of the internal window buffer to its new location */
        (void)current_wnd;
      }
    }
  }
}

/*
 * Yay, we are weird!
 * We will try to manage window creation, movement, minimalisation, ect. directly from kernelspace.
 * To supplement this, this driver will spawn helper threads, provide concise messaging, ect.
 *
 * When a process wants to get a window, it can use the GFX library to request one. The GFX library in turn
 * will send an IOCTL through a syscall with which it contacts the lwnd driver. This driver knows all 
 * about the current active video driver, so all we need to do is keep track of the pixels, and the windows.
 */
int init_window_driver()
{
  lwnd_screen_t* main_screen;

  _main_screen = NULL;
  memset(&_fb_info, 0, sizeof(_fb_info));

  _screens = create_vector(16, sizeof(lwnd_screen_t*), VEC_FLAG_NO_DUPLICATES);

  if (!_screens)
    return -1;

  /*
   * Try and get framebuffer info from the active video device 
   * TODO: when we implement 2D acceleration, we probably need to do something else here
   * TODO: implement screens on the drivers side with 'connectors'
   */
  driver_send_msg("core/video", VIDDEV_DCC_GET_FBINFO, &_fb_info, sizeof(_fb_info));

  main_screen = create_lwnd_screen(&_fb_info, LWND_SCREEN_MAX_WND_COUNT);

  if (!main_screen)
    return -1;

  vector_add(_screens, main_screen);

  ASSERT_MSG(spawn_thread("lwnd_main", lwnd_main, NULL), "Failed to create lwnd main thread");

  return 0;
}

int exit_window_driver()
{
  kernel_panic("TODO: exit window driver");
  return 0;
}

uintptr_t msg_window_driver(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  lwindow_t* window;
  proc_t* calling_process;

  calling_process = get_current_proc();

  if (!calling_process || size != sizeof(*window))
    return DRV_STAT_INVAL;

  if (IsError(kmem_validate_ptr(calling_process, (uintptr_t)buffer, size)))
    return DRV_STAT_INVAL;

  window = buffer;

  switch (code) {
    case LWND_DCC_CREATE:
      break;
  }
  /*
   * TODO: 
   * - Check what the process wants to do
   * - Check if the process is clear to do that thing
   * - do the thing
   * - fuck off
   */
  return DRV_STAT_OK;
}

/*
 * The Light window driver
 *
 * This 'service' will act as generic window manager in other systems, like linux, but we've packed
 * it in a kernel driver. This has a few advantages:
 *  o Better communication with the graphics drivers
 *  o Faster load times
 *  o Safer
 * It also has a few downsides or counterarguments
 *  o No REAL need to be a kernel component
 *  o Harder to customize
 */
EXPORT_DRIVER(window_driver) = {
  .m_name = "lwnd",
  .m_type = DT_SERVICE,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = init_window_driver,
  .f_exit = exit_window_driver,
  .f_msg = msg_window_driver,
  .m_dep_count = 1,
  .m_dependencies = { "core/video" },
};

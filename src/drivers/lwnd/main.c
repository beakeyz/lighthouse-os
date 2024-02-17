#include "dev/video/framebuffer.h"
#include "drivers/lwnd/alloc.h"
#include "drivers/lwnd/io.h"
#include "drivers/lwnd/screen.h"
#include "drivers/lwnd/window/app.h"
#include "drivers/lwnd/window/wallpaper.h"
#include "kevent/event.h"
#include "kevent/types/keyboard.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/video/device.h>
#include "sync/mutex.h"
#include "window.h"
#include "LibGfx/include/driver.h"

static fb_info_t _fb_info;
static lwnd_mouse_t _mouse;
static lwnd_keyboard_t _keyboard;

//static uint32_t _main_screen;
//static vector_t* _screens;
static lwnd_screen_t* main_screen;


/*!
 * @brief: Main compositing loop
 *
 * A few things need to happen here:
 * 1) Loop over the windows and draw visible stuff
 * 2) Queue events to windows that want them
 */
static void USED lwnd_main()
{
  bool recursive_update;
  lwnd_window_t* current_wnd;
  lwnd_screen_t* current_screen;

  (void)_mouse;
  (void)_keyboard;

  /* Launch an app to test our shit */
  create_test_app(main_screen);

  current_screen = main_screen;

  while (true) {

    recursive_update = false;

    /* Check for event BEFORE we draw */
    lwnd_screen_handle_events(current_screen);

    mutex_lock(current_screen->draw_lock);

    /* Loop over the stack from top to bottom */
    for (uint32_t i = current_screen->window_count-1;; i--) {
      current_wnd = current_screen->window_stack[i];

      if (!current_wnd)
        continue;

      /* Lock this window */
      mutex_lock(current_wnd->lock);
      
      /* Check if this windows wants to move */
      /* Clear the old bits in the pixel bitmap (These pixels will be overwritten with whatever is under them)*/
      /* Set the new bits in the pixel bitmap */
      /* Draw whatever is visible of the internal window buffer to its new location */

      /* If a window needs to sync, everything under it also needs to sync */
      if (!recursive_update)
        recursive_update = lwnd_window_should_redraw(current_wnd);
      else
        current_wnd->flags |= LWND_WNDW_NEEDS_REPAINT;

      /* Funky draw */
      lwnd_draw(current_wnd);

      /* Unlock this window */
      mutex_unlock(current_wnd->lock);

      if (!i)
        break;
    }

    mutex_unlock(current_screen->draw_lock);

    scheduler_yield();
  }
}

/*!
 * @brief: This is a temporary key handler to test out window event and shit
 *
 * TODO: send the key event over to the focussed window
 */
int on_key(kevent_ctx_t* ctx) 
{
  lwnd_window_t* wnd;
  kevent_kb_ctx_t* k_event;

  if (!main_screen || !main_screen->event_lock || mutex_is_locked(main_screen->event_lock))
    return 0;

  wnd = lwnd_screen_get_top_window(main_screen);

  if (!wnd)
    return 0;
 
  k_event = kevent_ctx_to_kb(ctx);

  /* TODO: save this instantly to userspace aswell */
  lwnd_save_keyevent(wnd, k_event);
  return 0;
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
  println("Initializing window driver!");

  int error;

  memset(&_fb_info, 0, sizeof(_fb_info));

  println("Initializing allocators!");
  error = init_lwnd_alloc();

  if (error)
    return -1;

  /*
   * Try and get framebuffer info from the active video device 
   * TODO: when we implement 2D acceleration, we probably need to do something else here
   * TODO: implement screens on the drivers side with 'connectors'
   */
  //Must(driver_send_msg_a("core/video", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &_fb_info, sizeof(_fb_info)));
  kernel_panic("TODO: fix lwnd");

  /* TODO: register to I/O core */
  kevent_add_hook("keyboard", "lwnd", on_key);

  println("Initializing screen!");
  main_screen = create_lwnd_screen(&_fb_info, LWND_SCREEN_MAX_WND_COUNT);

  if (!main_screen)
    return -1;

  println("Initializing wallpaper!");
  init_lwnd_wallpaper(main_screen);

  println("Starting deamon!");
  ASSERT_MSG(spawn_thread("lwnd_main", lwnd_main, NULL), "Failed to create lwnd main thread");

  return 0;
}

int exit_window_driver()
{
  kernel_panic("TODO: exit window driver");
  return 0;
}

/*!
 * @brief: Main lwnd message endpoint
 *
 * Every dc message to lwnd should have a lwindow in its buffer. If this is not the case, we instantly
 * reject the message
 */
uintptr_t msg_window_driver(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  window_id_t lwnd_id;
  lwnd_window_t* internal_wnd;
  lwindow_t* uwindow;
  proc_t* calling_process;

  calling_process = get_current_proc();

  /* Check size */
  if (!calling_process || size != sizeof(*uwindow))
    return DRV_STAT_INVAL;

  /* Check pointer */
  if (IsError(kmem_validate_ptr(calling_process, (uintptr_t)buffer, size)))
    return DRV_STAT_INVAL;

  /* Unsafe assignment */
  uwindow = buffer;

  switch (code) {

    case LWND_DCC_CREATE:

      /* 
       * Create the window while we know we aren't drawing anything 
       * NOTE: This takes the draw lock
       */
      lwnd_id = create_app_lwnd_window(main_screen, 0, 0, uwindow, calling_process);

      if (lwnd_id == LWND_INVALID_ID)
        return DRV_STAT_INVAL;

      uwindow->wnd_id = lwnd_id;
      break;

    case LWND_DCC_REQ_FB:
      mutex_lock(main_screen->draw_lock);

      /* Grab the window */
      internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

      mutex_unlock(main_screen->draw_lock);

      if (!internal_wnd)
        return DRV_STAT_INVAL;

      uwindow->fb = internal_wnd->user_fb_ptr;
      break;

    case LWND_DCC_UPDATE_WND:
      mutex_lock(main_screen->draw_lock);

      internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

      if (!internal_wnd) {
        mutex_unlock(main_screen->draw_lock);
        return DRV_STAT_INVAL;
      }

      lwnd_window_update(internal_wnd);

      mutex_unlock(main_screen->draw_lock);
      break;
    case LWND_DCC_GETKEY:
      /* Grab the window */
      internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

      lkey_event_t* u_event;
      kevent_kb_ctx_t kbd_ctx;

      if (lwnd_load_keyevent(internal_wnd, &kbd_ctx))
        return DRV_STAT_INVAL;

      /* Grab a buffer entry */
      u_event = &uwindow->keyevent_buffer[uwindow->keyevent_buffer_write_idx++];

      /* Write the data */
      u_event->keycode = kbd_ctx.keycode;
      u_event->pressed_char = kbd_ctx.pressed_char;
      u_event->pressed = kbd_ctx.pressed;
      u_event->mod_flags = NULL;

      println(to_string(kbd_ctx.keycode));

      /* Make sure we cycle the index */
      uwindow->keyevent_buffer_write_idx %= uwindow->keyevent_buffer_capacity;

      break;
  }
  /*
   * TODO: 
   * - Check what the process wants to do
   * - Check if the process is cleared to do that thing
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
};

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};

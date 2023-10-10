#include "dev/video/framebuffer.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/video/device.h>
#include "window.h"
#include "LibGfx/include/driver.h"

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
  fb_info_t fb_info = { 0 };

  driver_send_msg("core/video", VIDDEV_DCC_GET_FBINFO, &fb_info, sizeof(fb_info));

  return 0;
}

int exit_window_driver()
{
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

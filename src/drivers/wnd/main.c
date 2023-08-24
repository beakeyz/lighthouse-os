#include <dev/core.h>
#include <dev/driver.h>

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
  return 0;
}

int exit_window_driver()
{
  return 0;
}

#define LWND_DCC_CREATE 10
#define LWND_DCC_CLOSE 11
#define LWND_DCC_MINIMIZE 12
#define LWND_DCC_MOVE 13

uintptr_t msg_window_driver(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
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
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};

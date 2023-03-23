#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

#include "dev/handle.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "proc/ipc/packet_response.h"
#include <libk/stddef.h>

struct aniva_driver;
struct dev_manifest;

#define DRIVER_URL_SEPERATOR '.'

#define DRIVER_TYPE_COUNT 7

#define DRIVER_WAIT_UNTIL_READY (size_t)-1

typedef enum dev_type {
  DT_DISK = 0,
  DT_IO,
  DT_SOUND,
  DT_GRAPHICS,
  DT_SERVICE,
  DT_DIAGNOSTICS,
  DT_OTHER
} DEV_TYPE;

typedef enum dev_flags {
  STANDARD = (1 << 0),
  FROM_FILE = (1 << 1),
  LOADED_WITH_WARNIGN = (1 << 2),
} DEV_FLAGS;

const static char* dev_type_urls[DRIVER_TYPE_COUNT] = {
  [DT_DISK] = "disk",
  [DT_IO] = "io",
  [DT_SOUND] = "sound",
  [DT_GRAPHICS] = "graphics",
  [DT_OTHER] = "other",
  [DT_DIAGNOSTICS] = "diagnostics",
  [DT_SERVICE] = "service",
};

/*
 * Used by drivers to communicate, but also to send generic 
 * messages through sockets (i.e. some ccs are global, meaning they
 * are intercepted by the socket message dispatcher and interpreted)
 */
typedef uint32_t driver_control_code_t;

// TODO: global dc codes 
#define DCC_EXIT (uint32_t)(-1)

typedef const char* dev_url_t;

#define MAX_DRIVER_NAME_LENGTH 32
#define MAX_DRIVER_DESCRIPTOR_LENGTH 256

#define SOCKET_VERIFY_RESPONSE_SIZE(size) ((size) != ((size_t)-1))

/*
 * Initialize the driver registry. THIS MAY NOT ADD/BOOTSTRAP ANY
 * DRIVERS YET as the scheduler is not yet initialized and we may
 * still be in the boot context
 */
void init_aniva_driver_registry();

// TODO: load driver from file

/*
 * Registers the driver so it can be loaded
 */
ErrorOrPtr install_driver(struct dev_manifest* manifest);

/*
 * Unregisters the driver and also unloads it if it is still loaded
 */
ErrorOrPtr uninstall_driver(struct dev_manifest* manifest);

/*
 * load a driver from its structure in RAM
 */
ErrorOrPtr load_driver(struct dev_manifest* driver);

/*
 * unload a driver from its structure in RAM
 */
ErrorOrPtr unload_driver(dev_url_t url);

/*
 * Check if the driver is installed into the grid
 */
bool is_driver_installed(struct aniva_driver* handle);

/*
 * Check if the driver is loaded and currently active
 */
bool is_driver_loaded(struct aniva_driver* handle);

/*
 * Find the handle to a driver through its url
 */
struct dev_manifest* get_driver(dev_url_t url);

/*
 * Marks the driver as ready to recieve packets preemptively
 */
ErrorOrPtr driver_set_ready(const char* path);

/*
 * Find the url for a certain dev_type
 */
static ALWAYS_INLINE const char* get_driver_type_url(DEV_TYPE type) {
  return dev_type_urls[type];
}

/*
 * Resolve the drivers socket and send a packet to that port
 */
async_ptr_t* driver_send_packet(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);

/*
 * Same as above, but calls the requested function instantly, rather than waiting for the socket to 
 * Be scheduled so it can handle our packet.
 *
 * This fails if the socket is not set up yet
 */
packet_response_t* driver_send_packet_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);

/*
 * Sends a packet to the target driver and waits a number of scheduler yields for the socket to be ready
 * if the timer runs out before the socket is ready, we return nullptr
 * 
 * When mto is set to DRIVER_WAIT_UNTIL_READY, we simply wait untill the driver marks itself ready
 */
packet_response_t* driver_send_packet_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto);

#define DRIVER_VERSION(major, minor, bmp) {.maj = major, .min = minor, .bump = bmp} 

#define DRIVER_IDENT(major, minor) {.m_major = major, .m_minor = minor} 

#endif //__ANIVA_KDEV_CORE__

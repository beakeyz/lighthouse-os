#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

#include "libk/flow/error.h"
#include "proc/ipc/packet_response.h"
#include <libk/stddef.h>

struct aniva_driver;
struct dev_manifest;

#define DRIVER_URL_SEPERATOR '/'

#define DRIVER_TYPE_COUNT 8

#define DRIVER_WAIT_UNTIL_READY (size_t)-1

typedef enum dev_type {
  DT_DISK = 0,
  DT_FS,
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
  LOADED_WITH_WARNING = (1 << 2),
} DEV_FLAGS;

typedef struct dev_constraint {
  DEV_TYPE type;
  uint32_t max_count;
  uint32_t current_count;
} dev_constraint_t;

#define DRV_INFINITE            0xFFFFFFFF
#define DRV_SERVICE_MAX         0x100
#define DEV_MANIFEST_SOFTMAX    512

/* Defined in dev/core.c */
extern const char* dev_type_urls[DRIVER_TYPE_COUNT];

/*
 * Used by drivers to communicate, but also to send generic 
 * messages through sockets (i.e. some ccs are global, meaning they
 * are intercepted by the socket message dispatcher and interpreted)
 */
typedef const char*                     dev_url_t;

typedef uint32_t                        driver_control_code_t;

/* Welcome to the */
typedef driver_control_code_t           dcc_t;

/* Global driver control codes */
#define DCC_DATA                        (uint32_t)(0)
#define DCC_EXIT                        (uint32_t)(-1)
#define DCC_RESPONSE                    (uint32_t)(-2)
#define DCC_WRITE                       (uint32_t)(-3)

#define MAX_DRIVER_NAME_LENGTH          128
#define MAX_DRIVER_DESCRIPTOR_LENGTH    64  /* Descriptions should be short, simple and effective */

#define SOCKET_VERIFY_RESPONSE_SIZE(size) ((size) != ((size_t)-1))

#define EXPORT_DRIVER_PTR(name) static USED SECTION(".kpcdrvs") aniva_driver_t* exported_##name = (aniva_driver_t*)&name

#define EXPORT_DRIVER(name) static USED SECTION(".expdrv") ALIGN(8) aniva_driver_t name

/*
#define DRIVER_NAME(name) static const char* SECTION(".drv_name") __exported_drv_name USED = (const char*)(name)

#define DRIVER_FLAGS(flags) static const uint32_t SECTION(".drv_flags") __exported_drv_flags USED = (uint32_t)(flags)
#define DRIVER_MSG(msg_fn) static FuncPtr SECTION(".drv_msg") __exported_msg_fn USED = (FuncPtr)(msg_fn)
#define DRIVER_INIT(init_fn) static FuncPtr SECTION(".drv_init") __exported_init_fn USED = (FuncPtr)(init_fn)
#define DRIVER_EXIT(exit_fn) static FuncPtr SECTION(".drv_exit")__exported_exit_fn USED = (FuncPtr)(exit_fn)
*/

#define NO_MANIFEST NULL

/*
 * Initializes the registry for kernel drivers. 
 * Also bootstraps any drivers that came precompiled 
 * with the kernel
 */
void init_aniva_driver_registry();

// TODO: load driver from file

struct dev_manifest* allocate_dmanifest();

void free_dmanifest(struct dev_manifest* manifest);

/*
 * Registers the driver so it can be loaded
 */
ErrorOrPtr install_driver(struct aniva_driver* driver);

/*
 * Unregisters the driver and also unloads it if it is still loaded
 */
ErrorOrPtr uninstall_driver(struct dev_manifest* handle);

/*
 * load a driver from its structure in RAM
 */
ErrorOrPtr load_driver(struct dev_manifest* manifest);

/*
 * unload a driver from its structure in RAM
 */
ErrorOrPtr unload_driver(dev_url_t url);

/*
 * Check if the driver is installed into the grid
 */
bool is_driver_installed(struct dev_manifest* handle);

/*
 * Check if the driver is loaded and currently active
 */
bool is_driver_loaded(struct dev_manifest* handle);

/*
 * Find the handle to a driver through its url
 */
struct dev_manifest* get_driver(dev_url_t url);

bool verify_driver(struct aniva_driver* driver);

/*
 * This tries to get a driver from the loaded pool. If it is not
 * found, we see if it is installed and if it allows for dynamic 
 * loading. If the DRV_ALLOW_DYNAMIC_LOADING bit is set in both
 * the driver and the method, we try to load the driver anyway
 */
struct dev_manifest* try_driver_get(struct aniva_driver* driver, uint32_t flags);

/*
 * Marks the driver as ready to recieve packets preemptively
 */
ErrorOrPtr driver_set_ready(const char* path);

const char* driver_get_type_str(struct dev_manifest* driver);

/*
 * Find the url for a certain dev_type
 */
static ALWAYS_INLINE const char* get_driver_type_url(DEV_TYPE type) {
  return dev_type_urls[type];
}

/*
 * Resolve the drivers socket and send a packet to that port
 */
ErrorOrPtr driver_send_packet(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size);
ErrorOrPtr driver_send_packet_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t* resp_buffer_size);
ErrorOrPtr driver_send_packet_ex(struct dev_manifest* driver, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t* resp_buffer_size);

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

extern const char* get_driver_url(struct aniva_driver* handle);

extern size_t get_driver_url_length(struct aniva_driver* handle);

#define DRIVER_VERSION(major, minor, bmp) {.maj = major, .min = minor, .bump = bmp} 

#endif //__ANIVA_KDEV_CORE__

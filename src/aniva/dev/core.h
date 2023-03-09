#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

#include "dev/handle.h"
#include "libk/error.h"
#include <libk/stddef.h>

struct aniva_driver;
struct dev_manifest;

typedef enum DRIVER_LOAD_UNLOAD_INIT_METHOD {
  CALL,
  QUEUE
} DRIVER_LOAD_UNLOAD_INIT_METHOD_t;

typedef enum DEV_TYPE {
  DT_DISK,
  DT_IO,
  DT_SOUND,
  DT_GRAPHICS,
  DT_SERVICE,
  DT_DIAGNOSTICS,
  DT_OTHER
} DEV_TYPE_t;

typedef const char* dev_url_t;

#define MAX_DRIVER_NAME_LENGTH 32
#define MAX_DRIVER_DESCRIPTOR_LENGTH 256

/*
 * Initialize the driver registry. THIS MAY NOT ADD/BOOTSTRAP ANY
 * DRIVERS YET as the scheduler is not yet initialized and we may
 * still be in the boot context
 */
void init_aniva_driver_registry();

/*
 * load and bootstrap all the drivers we need for further booting
 * these are mostly generic drivers that we use untill we can load
 * more advanced and specific drivers for the hardware we use
 */
void register_aniva_base_drivers();

// TODO: load driver from file

/*
 * load a driver from its structure in RAM
 */
ErrorOrPtr load_driver(struct dev_manifest* driver);

/*
 * unload a driver from its structure in RAM
 */
ErrorOrPtr unload_driver(struct dev_manifest* driver);

/*
 * Check if the driver is installed into the grid
 */
bool is_driver_loaded(struct aniva_driver* handle);

/*
 * Find the handle to a driver through its url
 */
struct dev_manifest* get_driver(dev_url_t url);

/*
 * Resolve the drivers socket and send a packet to that port
 */
ErrorOrPtr driver_send_packet(const char* path, void* buffer, size_t buffer_size);

#define DRIVER_VERSION(major, minor, bmp) {.maj = major, .min = minor, .bump = bmp} 

#define DRIVER_IDENT(major, minor) {.m_major = major, .m_minor = minor} 

#endif //__ANIVA_KDEV_CORE__

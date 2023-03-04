#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

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

void init_aniva_driver_registry();

// TODO: load driver from file

/*
 * load a driver from its structure in RAM
 */
void load_driver(struct dev_manifest* driver);

/*
 * unload a driver from its structure in RAM
 */
void unload_driver(struct dev_manifest* driver);

bool is_driver_loaded(struct aniva_driver* handle);

#endif //__ANIVA_KDEV_CORE__

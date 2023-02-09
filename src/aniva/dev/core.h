#ifndef __ANIVA_KDEV_CORE__
#define __ANIVA_KDEV_CORE__

struct aniva_driver;

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

#define MAX_DRIVER_NAME_LENGTH 32
#define MAX_DRIVER_DESCRIPTOR_LENGTH 256

void init_aniva_driver_register();

// TODO: load driver from file

/*
 * load a driver from its structure in RAM
 */
void load_driver(struct aniva_driver* driver);

/*
 * unload a driver from its structure in RAM
 */
void unload_driver(struct aniva_driver* driver);

#define REGISTER_DRIVER(name, descriptor, version, major, minor, init, entry, exit, drv_msg) \
  struct aniva_driver aniva_drvr_skeleton = {                                          \
  .m_name = (name),                                                           \
  .m_descriptor = (descriptor),                                               \
  .m_version = (version),                                                       \
  .n_ident = {                                                                \
    .m_major = (major),                                                         \
    .m_minor = (minor),                                                         \
  },                                                                                \
  .f_init = (init)                                                                  \
  .f_entry = (entry),                                                           \
  .f_exit = (exit),                                                                          \
  .f_drv_msg = (drv_msg)                                                        \
  .m_type = DRIVER                                                             \
  }

#endif //__ANIVA_KDEV_CORE__

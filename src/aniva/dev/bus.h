#ifndef __ANIVA_DEV_BUS__
#define __ANIVA_DEV_BUS__

/*
 * The aniva bus management
 *
 * Here we'll be able to abstract away the complications of managing devices on busses
 */

#include <libk/stddef.h>

struct dgroup;

/*
 * The device bus
 * This is a structure that manages the devices that are attached to a bus
 */
typedef struct dbus {
  uint32_t busnum;
  struct dgroup* group;
} dbus_t;

#endif // !__ANIVA_DEV_BUS__

#ifndef __ANIVA_DEVICE_ENDPOINT__
#define __ANIVA_DEVICE_ENDPOINT__

/*
 * Define a generic device endpoint
 *
 * Devices can have multiple endpoints attached to them. It is up to the device driver to implement these endpoints.
 * These act as 'portals' between specific devices handled by a device driver and the software that wants to make use
 * of the device. Code in this header will therefore be directed at driver code, while any code that deals with the 
 * useage-side of devices should be put in user.h
 *
 * Functions here should be used by device drivers
 * Functions in user.h should be used by the rest of the system to (indirectly) invoke device services
 *
 * NOTE: user.h is a collection of shit that has to do with driver/device access from any outside source. This means
 * that when driver want to use other driver or devices they also interact with user.h
 */

#include "dev/core.h"
#include "devacs/shared.h"
#include <libk/stddef.h>

enum ENDPOINT_TYPE {
  ENDPOINT_TYPE_INVALID = NULL,
  ENDPOINT_TYPE_GENERIC,
  ENDPOINT_TYPE_DISK,
  ENDPOINT_TYPE_VIDEO,
  ENDPOINT_TYPE_HID,
  ENDPOINT_TYPE_PWM,
};

struct device;
struct device_generic_endpoint;
struct device_disk_endpoint;
/* Defined in video/device.h */
struct device_video_endpoint;
/* Defined in hid/hid.h */
struct device_hid_endpoint;
struct device_pwm_endpoint;

/*
 * Fully static struct
 *
 * This should never be allocated on the heap or any other form
 * of dynamic memory
 */
typedef struct device_endpoint {
  enum ENDPOINT_TYPE type;
  uint32_t size;
  union {
    void* priv;
    struct device_generic_endpoint* generic;
    struct device_disk_endpoint* disk;
    struct device_video_endpoint* video;
    struct device_hid_endpoint* hid;
    struct device_pwm_endpoint* pwm;
  } impl;

  struct device_endpoint* next;
} device_ep_t;

#define DEVICE_ENDPOINT(type, impl) { (type), sizeof((impl)), { &(impl) } }

static inline bool dev_is_valid_endpoint(device_ep_t* ep)
{
  return (ep && ep->impl.priv && ep->size);
}

struct device_generic_endpoint {
  /* Called once when the driver creates a device as the result of a probe of some sort */
  int (*f_create)(struct device* device);
  int (*f_destroy)(struct device* device);

  int (*f_disable)(struct device* device);
  int (*f_enable)(struct device* device);

  int (*f_get_devinfo)(struct device* device, DEVINFO* binfo);

  uintptr_t (*f_msg)(struct device* device, dcc_t code);
  uintptr_t (*f_msg_ex)(struct device* device, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

  int (*f_read)(struct device* dev, void* buffer, uintptr_t offset, size_t size);
  int (*f_write)(struct device* dev, void* buffer, uintptr_t offset, size_t size);
};

struct device_disk_endpoint {
  int (*f_bread)(struct device* dev, void* buffer, uintptr_t lba, size_t count);
  int (*f_bwrite)(struct device* dev, void* buffer, uintptr_t lba, size_t count);

  int (*f_flush)(struct device* dev);
};

struct device_pwm_endpoint {
  /* 'pm' opperations which are purely software, except if we support actual PM */
  int (*f_power_on)(struct device* device);
  int (*f_remove)(struct device* device);
  int (*f_suspend)(struct device* device);
  int (*f_resume)(struct device* device);
};

#endif // !__ANIVA_DEVICE_ENDPOINT__

#ifndef __ANIVA_HID_CORE__
#define __ANIVA_HID_CORE__

#include "dev/device.h"
#include "dev/driver.h"
#include "event.h"
#include "libk/flow/error.h"
#include <libk/stddef.h>

/*
 * The Aniva HID subsystem
 */

struct device;
struct hid_device;

#define HID_KEVENTNAME "hid"
#define HID_DEFAULT_EBUF_CAPACITY 16

enum HID_BUS_TYPE {
    HID_BUS_TYPE_PS2 = 0,
    HID_BUS_TYPE_USB,
};

typedef struct hid_device {
    /* Generic device */
    struct device* dev;
    enum HID_BUS_TYPE bus_type;

    /*
     * This is where we link any events that this device generates
     */
    hid_event_buffer_t device_events;
} hid_device_t;

void init_hid();

hid_device_t* create_hid_device(driver_t* driver, const char* name, enum HID_BUS_TYPE btype, device_ctl_node_t* ctllist);
void destroy_hid_device(hid_device_t* device);

kerror_t register_hid_device(hid_device_t* device);
kerror_t unregister_hid_device(hid_device_t* device);
hid_device_t* get_hid_device(const char* name);

kerror_t hid_device_queue(hid_device_t* device, struct hid_event* event);
kerror_t hid_device_poll(hid_device_t* device, struct hid_event** p_event);
kerror_t hid_device_flush(hid_device_t* device);

kerror_t hid_poll(struct hid_event** p_event);

/*
 * Can be called by HID drivers when they need
 * i8042 (emulation) to be disabled
 */
kerror_t hid_disable_i8042();

#endif // !__ANIVA_HID_CORE__

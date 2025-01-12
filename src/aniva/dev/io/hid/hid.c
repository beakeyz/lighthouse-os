#include "hid.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/group.h"
#include "dev/io/hid/event.h"
#include "lightos/api/device.h"
#include "kevent/event.h"
#include "libk/flow/error.h"
#include "mem/heap.h"

static dgroup_t* _hid_group;

/*!
 * @brief Initialize the HID subsystem
 */
void init_hid()
{
    _hid_group = register_dev_group(DGROUP_TYPE_MISC, "hid", NULL, NULL);

    /* Register a kernel event */
    ASSERT_MSG(KERR_OK(add_kevent(HID_KEVENTNAME, KE_DEVICE_EVENT, NULL, 512)), "Failed to add HID kevent");
}

/*!
 * @brief: Create a simple human input device struct
 */
hid_device_t* create_hid_device(driver_t* driver, const char* name, enum HID_BUS_TYPE btype, device_ctl_node_t* ctllist)
{
    size_t event_capacity;
    device_t* device;
    hid_device_t* hiddev;
    hid_event_t* event_buffer;
    enum DEVICE_CTYPE ctype;

    event_capacity = HID_DEFAULT_EBUF_CAPACITY;

    hiddev = kmalloc(sizeof(*hiddev));

    if (!hiddev)
        return nullptr;

    event_buffer = kmalloc(event_capacity * sizeof(hid_event_t));

    if (!event_buffer) {
        kfree(hiddev);
        return nullptr;
    }

    /* First, transform the btype into a ctype  */
    ctype = hid_get_ctype(btype);

    /* Then create a device with the resulting type */
    device = create_device_ex(driver, (char*)name, hiddev, ctype, NULL, ctllist);

    if (!device) {
        kfree(hiddev);
        kfree(event_buffer);
        return nullptr;
    }

    hiddev->dev = device;
    hiddev->bus_type = btype;

    /* Initialize the devices event buffer */
    init_hid_event_buffer(&hiddev->device_events, event_buffer, event_capacity);

    return hiddev;
}

/*!
 * @brief: Destroy a HID device
 */
void destroy_hid_device(hid_device_t* device)
{
    if (!device)
        return;

    destroy_device(device->dev);

    kfree(device);
}

/*!
 * @brief: Register a HID device
 */
kerror_t register_hid_device(hid_device_t* device)
{
    if (!device)
        return -KERR_INVAL;

    return device_register(device->dev, _hid_group);
}

/*!
 * @brief: Unregister a HID device
 */
kerror_t unregister_hid_device(hid_device_t* device)
{
    if (!device)
        return -KERR_INVAL;

    return device_unregister(device->dev);
}

/*!
 * @brief: Find a HID device
 */
hid_device_t* get_hid_device(const char* name)
{
    device_t* bdev;

    if (!KERR_OK(dev_group_get_device(_hid_group, name, &bdev)))
        return nullptr;

    if (!bdev)
        return nullptr;

    /* hihi assume this is HID =) */
    return bdev->private;
}

kerror_t hid_device_queue(hid_device_t* device, struct hid_event* event)
{
    if (!device || !event)
        return -1;

    return hid_event_buffer_write(&device->device_events, event);
}

/*!
 * @brief: Send a poll CTLC to the device
 *
 * TODO: This is currently unused, because the poll control code
 * for hid devices is also unused xD
 */
static inline int _hid_device_do_poll(hid_device_t* device)
{
    return device_send_ctl(device->dev, DEVICE_CTLC_HID_POLL);
}

kerror_t hid_device_poll(hid_device_t* device, struct hid_event** p_event)
{
    if (!device || !p_event)
        return -KERR_INVAL;

    /* If there is no new data */
    if (device->device_events.r_idx == device->device_events.w_idx)
        return KERR_NOT_FOUND;

    *p_event = hid_event_buffer_read(&device->device_events, &device->device_events.r_idx);

    if (*p_event)
        return 0;

    return -KERR_NOT_FOUND;
}

kerror_t hid_device_flush(hid_device_t* device)
{
    if (!device)
        return -KERR_INVAL;

    memset(device->device_events.buffer, 0, device->device_events.capacity * sizeof(hid_event_t));

    device->device_events.r_idx = device->device_events.w_idx;
    return 0;
}

kerror_t hid_disable_i8042()
{
    kerror_t error;
    hid_device_t* i8042;
    driver_t* driver;

    i8042 = get_hid_device("i8042");

    /* Already disabled */
    if (!i8042)
        return 0;

    driver = get_driver("io/i8042");

    if (!driver)
        return 0;

    error = uninstall_driver(driver);

    if (error)
        return error;

    i8042 = get_hid_device("i8042");

    /* If we can still find the device, there is definitely something wrong here */
    if (i8042)
        return -KERR_DEV;

    return 0;
}

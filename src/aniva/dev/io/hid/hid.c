#include "hid.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "dev/io/hid/event.h"
#include "kevent/event.h"
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
hid_device_t* create_hid_device(const char* name, enum HID_BUS_TYPE btype, struct device_endpoint* eps)
{
    size_t event_capacity;
    device_t* device;
    hid_device_t* hiddev;
    hid_event_t* event_buffer;

    event_capacity = HID_DEFAULT_EBUF_CAPACITY;

    hiddev = kmalloc(sizeof(*hiddev));

    if (!hiddev)
        return nullptr;

    event_buffer = kmalloc(event_capacity * sizeof(hid_event_t));

    if (!event_buffer) {
        kfree(hiddev);
        return nullptr;
    }

    device = create_device_ex(NULL, (char*)name, hiddev, NULL, eps);

    if (!device) {
        kfree(hiddev);
        kfree(event_buffer);
        return nullptr;
    }

    if (!device_has_endpoint(device, ENDPOINT_TYPE_HID)) {
        destroy_device(device);
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

    /* hihi assume this is HID =) */
    return bdev->private;
}

kerror_t hid_device_queue(hid_device_t* device, struct hid_event* event)
{
    return hid_event_buffer_write(&device->device_events, event);
}

static inline int _hid_device_do_poll(hid_device_t* device)
{
    device_ep_t* ep;

    ep = device_get_endpoint(device->dev, ENDPOINT_TYPE_HID);

    /* Check params */
    if (!ep || !ep->impl.hid || !ep->impl.hid->f_poll)
      return -1;

    /* Call the device poll */
    return ep->impl.hid->f_poll(device->dev);
}

kerror_t hid_device_poll(hid_device_t* device, struct hid_event** p_event)
{
    kerror_t error;

    error = 0;

    /* If there is no new data */
    if (device->device_events.r_idx == device->device_events.w_idx)
      error = _hid_device_do_poll(device);

    if (error)
      return error;

    *p_event = hid_event_buffer_read(&device->device_events, &device->device_events.r_idx);

    if (*p_event)
      return 0;

    return -KERR_NOT_FOUND;
}
kerror_t hid_device_flush(hid_device_t* device);


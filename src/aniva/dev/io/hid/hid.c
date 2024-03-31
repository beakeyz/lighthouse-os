#include "hid.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "mem/heap.h"

static dgroup_t* _hid_group;

/*!
 * @brief Initialize the HID subsystem
 */
void init_hid()
{
  _hid_group = register_dev_group(DGROUP_TYPE_MISC, "hid", NULL, NULL);
}

/*!
 * @brief: Create a simple human input device struct
 */
hid_device_t* create_hid_device(const char* name, enum HID_BUS_TYPE btype, struct device_endpoint* eps, uint32_t ep_count)
{
  device_t* device;
  hid_device_t* hiddev;

  hiddev = kmalloc(sizeof(*hiddev));

  if (!hiddev)
    return nullptr;

  device = create_device_ex(NULL, (char*)name, hiddev, NULL, eps, ep_count);

  if (!device || !device_has_endpoint(device, ENDPOINT_TYPE_HID)) {
    kfree(hiddev);
    return nullptr;
  }

  hiddev->dev = device;
  hiddev->btype = btype;
  hiddev->device_events = nullptr;

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

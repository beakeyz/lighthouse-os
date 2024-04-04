#include "hcd.h"
#include "dev/device.h"
#include "dev/usb/usb.h"
#include "sync/mutex.h"

/*!
 * @brief Allocate the needed memory for a USB hub structure
 *
 */
usb_hcd_t* create_usb_hcd(pci_device_t* host, char* hub_name, uint8_t type, struct device_endpoint *eps, uint32_t ep_count)
{
  usb_hcd_t* ret;

  ret = alloc_usb_hcd();

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(usb_hcd_t));

  ret->hub_name = hub_name;
  ret->hub_type = type;

  /* Start with a reference of one */
  ret->ref = 1;
  ret->pci_device = host;
  ret->pci_device->dev = create_device_ex(NULL, hub_name, ret, NULL, eps, ep_count);

  if (!ret->pci_device->dev)
    goto dealloc_and_exit;

  ret->hcd_lock = create_mutex(NULL);

  return ret;

dealloc_and_exit:
  dealloc_usb_hcd(ret);
  return nullptr;
}

/*!
 * @brief Destroy the hubs resources and the hub itself
 *
 */
void destroy_usb_hcd(usb_hcd_t* hub)
{
  destroy_device(hub->pci_device->dev);
  destroy_mutex(hub->hcd_lock);
  dealloc_usb_hcd(hub);
}

bool hcd_is_accessed(struct usb_hcd* hcd)
{
  return (hcd->ref);
}

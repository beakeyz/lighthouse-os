#include "hcd.h"
#include "dev/usb/usb.h"
#include "sync/mutex.h"

/*!
 * @brief Allocate the needed memory for a USB hub structure
 *
 */
usb_hcd_t* create_usb_hcd(pci_device_t* host, char* hub_name, uint8_t type)
{
  usb_hcd_t* ret;

  if (!is_valid_usb_hub_type(type))
    return nullptr;

  ret = alloc_usb_hcd();

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(usb_hcd_t));

  ret->devices = init_list();
  ret->child_hubs = init_list();

  ret->hcd_lock = create_mutex(NULL);

  ret->hub_name = hub_name;
  ret->hub_type = type;
  ret->host_device = host;

  /* Start with a reference of one */
  ret->ref = 1;
  ret->hub_idx = (uint8_t)-1;

  return ret;
}

/*!
 * @brief Destroy the hubs resources and the hub itself
 *
 */
void destroy_usb_hcd(usb_hcd_t* hub)
{
  destroy_list(hub->devices);
  destroy_list(hub->child_hubs);

  destroy_mutex(hub->hcd_lock);

  dealloc_usb_hcd(hub);
}

bool hcd_is_accessed(struct usb_hcd* hcd)
{
  return (hcd->ref);
}

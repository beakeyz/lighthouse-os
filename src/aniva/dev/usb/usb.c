/* Core USB functions */
#include "usb.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

zone_allocator_t __usb_hub_allocator;
usb_hcd_t* __root_hub = nullptr;

static bool is_valid_usb_hub_type(uint8_t type)
{
  return (type <= USB_HUB_TYPE_MAX);
}

/*!
 * @brief Allocate the needed memory for a USB hub structure
 *
 */
usb_hcd_t* create_usb_hcd(pci_device_t* host, char* hub_name, uint8_t type)
{
  usb_hcd_t* ret;

  if (!is_valid_usb_hub_type(type))
    return nullptr;

  ret = zalloc_fixed(&__usb_hub_allocator);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(usb_hcd_t));

  ret->devices = init_list();
  ret->child_hubs = init_list();

  ret->hub_name = hub_name;
  ret->hub_type = type;
  ret->host_device = host;

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

  zfree_fixed(&__usb_hub_allocator, hub);
}

/*!
 * @brief Registers a USB hub directly to the root
 */
int register_usb_hcd(usb_hcd_t* hub)
{
  if (!__root_hub)
    return -1;

  list_append(__root_hub->child_hubs, hub);

  hub->parent = __root_hub;

  return 0;
}

/*!
 * @brief Removes a USB hub from the root
 *
 * TODO: more advanced access tracking
 */
int unregister_usb_hcd(usb_hcd_t* hub)
{
  ErrorOrPtr result;
  uint32_t our_index;

  if (!__root_hub)
    return -1;

  result = list_indexof(__root_hub->child_hubs, hub);

  if (IsError(result))
    return -2;

  our_index = Release(result);

  list_remove(__root_hub->child_hubs, our_index);

  return -1;
}

/*!
 * @brief Common USB subsytem initialization
 */
void init_usb()
{
  Must(init_zone_allocator(&__usb_hub_allocator, 64 * Kib, sizeof(usb_hcd_t), NULL));

  __root_hub = create_usb_hcd(nullptr, "root_usb_hub", USB_HUB_TYPE_MOCK);
}

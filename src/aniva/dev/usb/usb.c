/* Core USB functions */
#include "usb.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/request.h"
#include "libk/data/linkedlist.h"
#include "libk/data/vector.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"

zone_allocator_t __usb_hub_allocator;
zone_allocator_t __usb_request_allocator;
vector_t* __usb_hcds;

/*!
 * @brief Allocate memory for a HCD
 *
 * Nothing to add here...
 */
usb_hcd_t* alloc_usb_hcd()
{
  usb_hcd_t* ret;

  ret = zalloc_fixed(&__usb_hub_allocator);

  memset(ret, 0, sizeof(usb_hcd_t));

  return ret;
}

/*!
 * @brief Deallocate memory of a HCD
 *
 * Nothing to add here...
 */
void dealloc_usb_hcd(struct usb_hcd* hcd)
{
  if (!hcd || !__usb_hcds)
    return;

  zfree_fixed(&__usb_hub_allocator, hcd);
}

/*!
 * @brief Allocate and initialize a generic USB device
 *
 */
usb_device_t* create_usb_device(usb_hub_t* hub, uint8_t port_num)
{
  usb_device_t* device;

  device = kmalloc(sizeof(*device));

  if (!device)
    return nullptr;

  memset(device, 0, sizeof(*device));

  device->req_doorbell = create_doorbell(255, NULL);
  device->port_num = port_num;
  device->hub = hub;

  /* TODO: get the devic descriptor n shit */

  kernel_panic("TODO: gather USB device info");

  return device;
}

/*!
 * @brief Deallocate a USB device
 *
 */
void destroy_usb_device(usb_device_t* device)
{
  kernel_panic("TODO: destroy_usb_device");
  destroy_doorbell(device->req_doorbell);
  kfree(device);
}


/*!
 * @brief Allocate and initialize a generic USB hub
 *
 * TODO: create our own root hub configuration descriptor
 */
usb_hub_t* create_usb_hub(struct usb_hcd* hcd, usb_hub_t* parent, uint8_t d_addr, uint8_t p_num)
{
  usb_hub_t* hub;

  hub = kmalloc(sizeof(*hub));

  if (!hub)
    return nullptr;

  hub->parent = parent;
  hub->hcd = hcd;
  hub->dev_addr = d_addr;
  hub->device = create_usb_device(hub, p_num);

  if (!hub->device) {
    kfree(hub);
    return nullptr;
  }

  return hub;
}

/*!
 * @brief Deallocate a generic USB hub
 *
 */
void destroy_usb_hub(usb_hub_t* hub)
{
  kernel_panic("TODO: destroy_usb_hub");
}


/*!
 * @brief Registers a USB hub directly to the root
 *
 * This only makes the hcd available for searches from the root hcd
 * (which is just a mock bridge) so we know where it is at all times
 */
int register_usb_hcd(usb_hcd_t* hub)
{
  uint8_t hub_idx;
  ErrorOrPtr result;
  int error = 0;

  if (!__usb_hcds || !hub->hw_ops || !hub->hw_ops->hcd_start)
    return -1;

  result = vector_add(__usb_hcds, hub);

  if (IsError(result))
    return -2;

  hub_idx = Release(result);

  mutex_lock(hub->hcd_lock);
  hub->hub_idx = hub_idx;
  mutex_unlock(hub->hcd_lock);

  /*
   * NOTE: after setup and stuff, there might be stuff we need to deallocate
   * so FIXME
   */

  if (hub->hw_ops->hcd_setup) {
    error = hub->hw_ops->hcd_setup(hub);

    if (error)
      goto fail_and_unregister;
  }

  error = hub->hw_ops->hcd_start(hub);

  if (error)
    goto fail_and_unregister;

  return 0;

fail_and_unregister:
  unregister_usb_hcd(hub);
  return error;
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

  if (!__usb_hcds)
    return -1;

  result = vector_indexof(__usb_hcds, hub);

  if (IsError(result))
    return -2;

  our_index = Release(result);

  result = vector_remove(__usb_hcds, our_index);

  if (IsError(result))
    return -3;

  mutex_lock(hub->hcd_lock);
  hub->hub_idx = (uint8_t)-1;
  mutex_unlock(hub->hcd_lock);

  return 0;
}

/*!
 * @brief Get the HCD at a certain index
 *
 * Grabs a pointer to the hcd and increments the access counter
 */
usb_hcd_t* get_usb_hcd(uint8_t index)
{
  usb_hcd_t* ret;

  ret = vector_get(__usb_hcds, index);

  if (!ret)
    return nullptr;

  flat_ref(&ret->ref);

  return ret;
}

/*!
 * @brief Get the HCD for a specific pci device
 *
 * Nothing to add here...
 */
usb_hcd_t* get_hcd_for_pci_device(pci_device_t* device)
{
  FOREACH_VEC(__usb_hcds, i, j) {
    usb_hcd_t* hcd = *(usb_hcd_t**)i;

    if (hcd->host_device == device) {

      flat_ref(&hcd->ref);

      return hcd;
    }
  }

  return nullptr;
}

/*!
 * @brief Release a reference to the hcd
 *
 * For now we only decrement the flat reference counter, but 
 * reference management is a massive subsytem TODO lol
 */
void release_usb_hcd(struct usb_hcd* hcd)
{
  flat_unref(&hcd->ref);

  /*
   * FIXME: do we also destroy the hcd when this hits zero? 
   * FIXME 2: We REALLY need to rethink our reference management model lmao
   */
}


usb_request_t* allocate_usb_request()
{
  usb_request_t* req;

  req = zalloc_fixed(&__usb_request_allocator);

  if (!req)
    return nullptr;

  memset(req, 0, sizeof(*req));

  return req;
}

void deallocate_usb_request(usb_request_t* req)
{
  if (!req)
    return;

  zfree_fixed(&__usb_request_allocator, req);
}

/*!
 * @brief Common USB subsytem initialization
 */
void init_usb()
{
  Must(init_zone_allocator(&__usb_hub_allocator, 16 * Kib, sizeof(usb_hcd_t), NULL));
  Must(init_zone_allocator(&__usb_request_allocator, 32 * Kib, sizeof(usb_request_t), NULL));

  /* Use a vector to store the usb host controller drivers */
  __usb_hcds = create_vector(MAX_USB_HCDS, sizeof(void*), VEC_FLAG_NO_DUPLICATES);

  ASSERT_MSG(__usb_hcds, "Failed to create vector for hcds");
}

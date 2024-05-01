#include "device.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include "endpoint.h"
#include <libk/string.h>

static device_ep_t* _create_device_ep(enum ENDPOINT_TYPE type, void* ep, size_t size)
{
  device_ep_t* dev_ep;

  if (!ep || !size)
    return nullptr;

  dev_ep = kmalloc(sizeof(*dev_ep));

  if (!dev_ep)
    return nullptr;

  memset(dev_ep, 0, sizeof(*dev_ep));

  dev_ep->type = type;
  dev_ep->size = size;
  dev_ep->impl.priv = ep;

  return dev_ep;
}

static void _destroy_device_ep(device_ep_t* ep)
{
  kfree(ep);
}

static struct device_endpoint** _device_get_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  device_ep_t** c_ep;
  
  if (!device || !device->endpoints)
    return nullptr;

  c_ep = &device->endpoints;

  do {
    if ((*c_ep)->type == type)
      return c_ep;

    c_ep = &(*c_ep)->next;
  } while(*c_ep);

  return nullptr;
}

/*!
 * @brief: Walk the endpoints of @device to check if there is an endpoint of @type present
 */
struct device_endpoint* device_get_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  device_ep_t** c_ep;

  mutex_lock(device->ep_lock);

  c_ep = _device_get_endpoint(device, type);

  mutex_unlock(device->ep_lock);

  if (!c_ep || !(*c_ep))
    return nullptr;

  return (*c_ep);
}

kerror_t device_unimplement_endpoint(device_t* device, enum ENDPOINT_TYPE type)
{
  device_ep_t* ptarget_ep;
  device_ep_t** target_ep;

  target_ep = _device_get_endpoint(device, type);

  if (!target_ep || !(*target_ep))
    return -KERR_NULL;

  mutex_lock(device->ep_lock);

  /* Cache the direct pointer */
  ptarget_ep = *target_ep;

  /* Link to next */
  *target_ep = ptarget_ep->next;

  /* Update device endpoint count */
  device->endpoint_count--;

  mutex_lock(device->ep_lock);

  /* Do cleanup */
  _destroy_device_ep(ptarget_ep);
  return KERR_NONE;
}

kerror_t device_implement_endpoint(device_t* device, enum ENDPOINT_TYPE type, void* ep, size_t ep_size)
{
  device_ep_t* dev_ep;

  if (!ep || !ep_size)
    return -KERR_INVAL;

  if (device_has_endpoint(device, type))
    return -KERR_INVAL;

  /* Try to create the device endpoint */
  dev_ep = _create_device_ep(type, ep, ep_size);
  
  if (!dev_ep)
    return -KERR_NOMEM;

  mutex_lock(device->ep_lock);

  /* Link the thing */
  dev_ep->next = device->endpoints;
  device->endpoints = dev_ep;

  /* Update device endpoint count */
  device->endpoint_count++;

  mutex_unlock(device->ep_lock);

  return KERR_NONE;
}

/*!
 * @brief: Implements an array of endpoints
 *
 * @returns: The amount of endpoints implemented on success, a negative error code otherwise
 */
kerror_t device_implement_endpoints(device_t* device, struct device_endpoint* endpoints)
{
  kerror_t error;
  int impl_count;
  device_ep_t* eps;

  if (!device || !endpoints)
    return -KERR_INVAL;

  error = 0;
  impl_count = 0;
  eps = endpoints;

  /* Implement all the specified endpoints */
  while (eps && eps->type != ENDPOINT_TYPE_INVALID) {
    error = device_implement_endpoint(device, eps->type, eps->impl.priv, eps->size);
    
    if (KERR_OK(error))
      impl_count++;
    
    eps++;
  }

  if (!impl_count)
    return -KERR_INVAL;

  return impl_count;
}

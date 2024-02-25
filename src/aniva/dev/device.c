#include "device.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "dev/manifest.h"
#include <libk/string.h>
#include "dev/video/device.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"

static oss_node_t* _device_node;

/*
 * TODO: how do we shape a 'generic' device?
 *
 * These functions all rely on an underlying driver that manages resources, because a 
 * device is not aware of it's own resources but it only serves as an intermediate / abstraction
 * to make communication with drivers (and thus hardware) easier
 */
static struct device_endpoint _generic_dev_eps[] = {
  { ENDPOINT_TYPE_GENERIC, NULL, { NULL } },
  { NULL }
};

device_t* create_device(aniva_driver_t* parent, char* name)
{
  return create_device_ex(parent, name, nullptr, NULL, _generic_dev_eps, 0);
}

/*!
 * @brief: Allocate memory for a device on @path
 *
 * TODO: create a seperate allocator that lives in dev/core.c for faster 
 * device (de)allocation
 *
 * NOTE: this does not register a device to a driver, which means that it won't have a parent
 */
device_t* create_device_ex(struct aniva_driver* parent, char* name, void* priv, uint32_t flags, struct device_endpoint* eps, uint32_t ep_count)
{
  device_t* ret;
  drv_manifest_t* parent_man;
  struct device_endpoint* g_ep;

  if (!name)
    return nullptr;

  parent_man = nullptr;

  if (parent)
    parent_man = try_driver_get(parent, NULL);

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  /* Make sure that a device always has generic opperations */
  if (!eps)
    eps = _generic_dev_eps;

  ret->name = strdup(name);
  ret->lock = create_mutex(NULL);
  ret->obj = create_oss_obj(ret->name);
  ret->driver = parent_man;
  ret->flags = flags;
  ret->private = priv;
  ret->endpoints = eps;
  ret->endpoint_count = ep_count;

  /* Make sure the object knows about us */
  oss_obj_register_child(ret->obj, ret, OSS_OBJ_TYPE_DEVICE, destroy_device);

  g_ep = device_get_endpoint(ret, ENDPOINT_TYPE_GENERIC);

  /* Call the private device constructor */
  if (dev_is_valid_endpoint(g_ep) && g_ep->impl.generic->f_create)
    g_ep->impl.generic->f_create(ret);

  /* Make sure we register ourselves to the driver */
  if (ret->driver)
    manifest_add_dev(ret->driver);

  return ret;
}

/*!
 * @brief: Destroy memory belonging to a device
 *
 * Any references to this device must be dealt with before this point, due
 * to the mutex being a bitch
 *
 * NOTE: devices don't have ownership of the memory of their parents EVER, thus
 * we don't destroy device->parent
 */
void destroy_device(device_t* device)
{
  if (device->obj && device->obj->priv == device) {
    destroy_oss_obj(device->obj);
    return;
  }

  /* Let the system know that there was a driver removed */
  if (device->driver)
    manifest_remove_dev(device->driver);

  destroy_mutex(device->lock);
  kfree((void*)device->name);

  memset(device, 0, sizeof(*device));

  kfree(device);
}

/*!
 * @brief: Add a new dgroup to the device node
 */
int device_node_add_group(struct oss_node* node, dgroup_t* group)
{
  if (!group || !group->node)
    return -1;

  if (!node)
    node = _device_node;

  return oss_node_add_node(node, group->node);
}

/*!
 * @brief: Registers a device to the device tree
 *
 * This assumes @group is already registered
 */
int device_register(device_t* dev, dgroup_t* group)
{
  oss_node_t* node;

  node = _device_node;

  if (group)
    node = group->node;

  return oss_node_add_obj(node, dev->obj);
}

/*!
 * @brief: Register a device to another devices which is a bus
 */
int device_register_to_bus(device_t* dev, device_t* busdev)
{
  dgroup_t* grp;

  if (!device_is_bus(busdev))
    return -KERR_INVAL;

  grp = busdev->bus_group;

  return device_register(dev, grp);
}

int device_unregister(device_t* dev)
{
  int error;
  dgroup_t* dgroup;
  oss_node_entry_t* entry;

  error = device_get_group(dev, &dgroup);

  if (error)
    return error;

  error = oss_node_remove_entry(dgroup->node, dev->obj->name, &entry);

  if (error)
    return error;

  destroy_oss_node_entry(entry);
  return 0;
}

static bool __device_itterate(oss_node_t* node, oss_obj_t* obj, void* arg)
{
  DEVICE_ITTERATE ittr = arg;

  /* If we have an object, we hope for it to be a device */
  if (obj && obj->type == OSS_OBJ_TYPE_DEVICE)
    return ittr(oss_obj_unwrap(obj, device_t));

  return true;
}

int device_for_each(struct dgroup* root, DEVICE_ITTERATE callback)
{
  oss_node_t* this;

  if (!callback)
    return -KERR_INVAL;

  /* Start itteration from the device root if there is no itteration root given through @root */
  this = _device_node;

  if (root)
    this = root->node;

  return oss_node_itterate(this, __device_itterate, callback);
}

/*!
 * @brief: Open a device from oss
 *
 * We enforce devices to be attached to the device root
 */
device_t* device_open(const char* path)
{
  int error;
  oss_node_t* obj_rootnode;
  oss_obj_t* obj;
  device_t* ret;

  error = oss_resolve_obj(path, &obj);

  if (error || !obj)
    return nullptr;

  obj_rootnode = oss_obj_get_root_parent(obj);

  ASSERT_MSG(obj_rootnode, "device_open: Somehow we found an object without a root parent which was still attached to the oss???");

  ret = oss_obj_get_device(obj);

  /* If the object is invalid we have to close it */
  if (!ret || obj_rootnode != _device_node) {
    oss_obj_close(obj);
    /* Make sure we return NULL no matter what */
    ret = nullptr;
  }

  return ret;
}

int device_close(device_t* dev)
{
  /* TODO: ... */
  return oss_obj_close(dev->obj);
}

kerror_t device_bind_driver(device_t* dev, struct drv_manifest* driver)
{
  if (dev->driver)
    return -KERR_INVAL;

  mutex_lock(dev->lock);
  dev->driver = driver;
  mutex_unlock(dev->lock);

  return KERR_NONE;
}

/*!
 * @brief: Get the group of a certain device
 *
 * @dev: The device we want to know the group of
 * @group_out: The output buffer for the group we find
 *
 * @returns: 0 on success, errorcode otherwise
 */
int device_get_group(device_t* dev, struct dgroup** group_out)
{
  oss_node_t* dev_parent_node;

  if (!dev || !group_out)
    return -1;

  dev_parent_node = dev->obj->parent;

  if (dev_parent_node->type != OSS_GROUP_NODE)
    return -2;

  /* This node is of type group node, so we may assume it's private field is a devicegroup */
  *group_out = dev_parent_node->priv;
  return 0;
}

/*
 * Device opperation wrapper functions
 *
 * These functions take away some responsibility from drivers to check and do certain things.
 *  - device_read and device_write are purely wrappers
 *  - device_remove will remove the device from it's parent after calling the devices ->d_remove func
 *  - device_suspend and device_resume are also wrappers
 *  - so are device_message and device_message_ex
 */

/*!
 * @brief: Read from a device
 *
 * Should only be implemented on device types where it makes sense. Read opperations should
 * be documented in such a way that there is no ambiguity
 *
 * Currently there are these endpoints that implement read opperations:
 *  - ENDPOINT_TYPE_DISK
 *
 * If a device has more than 1 of these endpoints, we panic the entire kernel (cuz why not)
 */
int device_read(device_t* dev, void* buffer, uintptr_t offset, size_t size)
{
  device_ep_t* ep;

  ep = device_get_endpoint(dev, ENDPOINT_TYPE_GENERIC);

  /* This would all be very bad news */
  if (!ep || !ep->impl.generic || !ep->impl.generic->f_read)
    return -KERR_NULL;

  return ep->impl.generic->f_read(dev, buffer, offset, size);
}

/*!
 * @brief: Write to a device
 *
 * Should only be implemented on device types where it makes sense. Write opperations should
 * be documented in such a way that there is no ambiguity
 */
int device_write(device_t* dev, void* buffer, uintptr_t offset, size_t size)
{
  device_ep_t* ep;

  ep = device_get_endpoint(dev, ENDPOINT_TYPE_GENERIC);

  /* This would all be very bad news */
  if (!ep || !ep->impl.generic || !ep->impl.generic->f_write)
    return -KERR_NULL;

  return ep->impl.generic->f_write(dev, buffer, offset, size);
}

int device_power_on(device_t* dev)
{
  kerror_t error;
  device_ep_t* pwm_ep;

  pwm_ep = device_get_endpoint(dev, ENDPOINT_TYPE_PWM);

  if (!pwm_ep || !pwm_ep->impl.pwm->f_power_on)
    return -1;

  /* Call endpoint */
  error = pwm_ep->impl.pwm->f_power_on(dev);

  if (error)
    return error;

  dev->flags |= DEV_FLAG_POWERED;
  return 0;
}

/*!
 * @brief: Remove a device
 *
 * Can be called both from software side as from hardware side. Called from software
 * when we want to remove a device from our register, called from hardware when a device
 * is hotplugged for example
 */
int device_on_remove(device_t* dev)
{
  device_ep_t* pwm_ep;

  pwm_ep = device_get_endpoint(dev, ENDPOINT_TYPE_PWM);

  if (!pwm_ep)
    return -1;

  return pwm_ep->impl.pwm->f_remove(dev);
}

/*!
 * @brief: ...
 */
int device_suspend(device_t* dev)
{
  kernel_panic("TODO: device_suspend");
}

/*!
 * @brief: ...
 */
int device_resume(device_t* dev)
{
  kernel_panic("TODO: device_resume");
}

/*!
 * @brief: ...
 */
uintptr_t device_message(device_t* dev, dcc_t code)
{
  kernel_panic("TODO: device_message");
}

/*!
 * @brief: ...
 */
uintptr_t device_message_ex(device_t* dev, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  kernel_panic("TODO: device_message_ex");
}

static void print_obj_path(oss_node_t* node) 
{
  if (node) {
    print_obj_path(node->parent);
    printf("/%s", node->name);
  }
}

static bool _itter(device_t* dev)
{
  oss_obj_t* obj = dev->obj;

  if (obj) {
    print_obj_path(obj->parent);
    printf("/%s\n", obj->name);
  }

  return true;
}

void debug_devices()
{
  device_for_each(NULL, _itter);

  kernel_panic("debug_devices");
}


/*!
 * @brief: Initialize the device subsystem
 *
 * The main job of the device subsystem is to allow for quick and easy device storage and access.
 * This is done through the oss, on the path 'Dev'.
 *
 * The first devices to be attached here are bus devices, firmware controllers, chipset/motherboard devices 
 * (PIC, PIT, CMOS, RTC, APIC, ect.)
 */
void init_devices()
{
  /* Initialize an OSS endpoint for device access and storage */
  _device_node = create_oss_node("Dev", OSS_OBJ_STORE_NODE, NULL, NULL);

  ASSERT_MSG(oss_attach_rootnode(_device_node) == 0, "Failed to attach device node");

  init_dgroups();

  /* Enumerate devices */
}

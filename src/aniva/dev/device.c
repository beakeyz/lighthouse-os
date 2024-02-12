#include "device.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "dev/manifest.h"
#include <libk/string.h>
#include "dev/video/device.h"
#include "libk/flow/error.h"
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

bool device_is_generic(device_t* device)
{
  return (device->endpoints == _generic_dev_eps);
}

device_t* create_device(aniva_driver_t* parent, char* path)
{
  return create_device_ex(parent, path, nullptr, NULL, _generic_dev_eps, 0);
}

/*!
 * @brief: Allocate memory for a device on @path
 *
 * TODO: create a seperate allocator that lives in dev/core.c for faster 
 * device (de)allocation
 *
 * NOTE: this does not register a device to a driver, which means that it won't have a parent
 */
device_t* create_device_ex(struct aniva_driver* parent, char* path, void* priv, uint32_t flags, struct device_endpoint* eps, uint32_t ep_count)
{
  device_t* ret;
  dev_manifest_t* parent_man;
  struct device_endpoint* g_ep;

  if (!path)
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

  ret->device_path = strdup(path);
  ret->lock = create_mutex(NULL);
  ret->obj = create_oss_obj(ret->device_path);
  ret->parent = parent_man;
  ret->flags = flags;
  ret->private = priv;
  ret->endpoints = eps;
  ret->endpoint_count = ep_count;

  /* Make sure the object knows about us */
  oss_obj_register_child(ret->obj, ret, OSS_OBJ_TYPE_DEVICE, destroy_device);

  g_ep = dev_get_endpoint(ret, ENDPOINT_TYPE_GENERIC);

  /* Call the private device constructor */
  if (dev_is_valid_endpoint(g_ep) && g_ep->impl.generic->f_create)
    g_ep->impl.generic->f_create(ret);

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

  destroy_mutex(device->lock);
  kfree((void*)device->device_path);

  memset(device, 0, sizeof(*device));

  kfree(device);
}

/*!
 * @brief: Add a new dgroup to the device node
 */
int device_add_group(dgroup_t* group, struct oss_node* node)
{
  if (!group || !group->node)
    return -1;

  if (!node)
    node = _device_node;

  return oss_node_add_node(node, group->node);
}

int device_register(device_t* dev)
{
  return oss_node_add_obj(_device_node, dev->obj);
}

static oss_obj_t* _device_open(oss_node_t* node, const char* path)
{
  return nullptr;
}

/*
 * Standard opperations for the device node
 */
static oss_node_ops_t _device_node_ops = {
  .f_open = _device_open,
  nullptr,
};

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
  _device_node = create_oss_node("Dev", OSS_OBJ_GEN_NODE, &_device_node_ops, NULL);

  ASSERT_MSG(oss_attach_rootnode(_device_node) == 0, "Failed to attach device node");

  init_dgroups();

  /* Enumerate devices */
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
 */
int device_read(device_t* dev, void* buffer, size_t size, uintptr_t offset)
{
  kernel_panic("TODO: device_read");
}

/*!
 * @brief: Write to a device
 *
 * Should only be implemented on device types where it makes sense. Write opperations should
 * be documented in such a way that there is no ambiguity
 */
int device_write(device_t* dev, void* buffer, size_t size, uintptr_t offset)
{
  kernel_panic("TODO: device_write");
}

/*!
 * @brief: Remove a device
 *
 * Can be called both from software side as from hardware side. Called from software
 * when we want to remove a device from our register, called from hardware when a device
 * is hotplugged for example
 */
int device_remove(device_t* dev)
{
  kernel_panic("TODO: device_remove");
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

static bool _itter(oss_node_t* node, oss_obj_t* obj)
{
  if (obj) {
    print_obj_path(obj->parent);
    printf("/%s\n", obj->name);
  }

  return true;
}

void devices_debug()
{
  oss_node_itterate(_device_node, _itter);

  kernel_panic("devices_debug");
}

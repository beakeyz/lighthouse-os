#include "device.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "dev/loader.h"
#include "dev/manifest.h"
#include "dev/misc/null.h"
#include "dev/usb/usb.h"
#include "dev/video/device.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"
#include "system/acpi/acpi.h"
#include <libk/string.h>

static oss_node_t* _device_node;

static void __destroy_device(device_t* device);

device_t* create_device(drv_manifest_t* parent, char* name, void* priv)
{
    return create_device_ex(parent, name, priv, NULL, NULL);
}

/*!
 * @brief: Allocate memory for a device on @path
 *
 * TODO: create a seperate allocator that lives in dev/core.c for faster
 * device (de)allocation
 *
 * NOTE: this does not register a device to a driver, which means that it won't have a parent
 * NOTE: @eps needs to be an array of endpoints, with a terminating entry with ->type = ENDPOINT_TYPE_INVALID
 */
device_t* create_device_ex(struct drv_manifest* parent, char* name, void* priv, uint32_t flags, struct device_endpoint* eps)
{
    device_t* ret;
    struct device_endpoint* g_ep;

    if (!name)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->name = strdup(name);
    ret->lock = create_mutex(NULL);
    ret->ep_lock = create_mutex(NULL);
    ret->obj = create_oss_obj(ret->name);
    ret->drivers[0] = parent;
    ret->flags = flags;
    ret->private = priv;
    ret->endpoints = NULL;
    ret->endpoint_count = NULL;

    /* Implement the device endpoints */
    (void)device_implement_endpoints(ret, eps);

    /* Make sure the object knows about us */
    oss_obj_register_child(ret->obj, ret, OSS_OBJ_TYPE_DEVICE, __destroy_device);

    /* Try to call f_create in the generic endpoint */
    g_ep = device_get_endpoint(ret, ENDPOINT_TYPE_GENERIC);

    /* Call the private device constructor */
    if (dev_is_valid_endpoint(g_ep) && g_ep->impl.generic->f_create)
        g_ep->impl.generic->f_create(ret);

    /* Make sure we register ourselves to the driver */
    if (parent)
        manifest_add_dev(parent, ret);

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
void __destroy_device(device_t* device)
{
    device_ep_t *c_ep, *next_ep;

    /* Let the system know that there was a driver removed */
    device_clear_drivers(device);

    /* Remove any remaining endpoints from the device */
    while (device->endpoints) {
        /* Cycle to next */
        c_ep = device->endpoints;
        next_ep = c_ep->next;

        /* Unimplement this endpoint */
        (void)device_unimplement_endpoint(device, c_ep->type);

        device->endpoints = next_ep;
    }

    destroy_mutex(device->lock);
    destroy_mutex(device->ep_lock);
    kfree((void*)device->name);

    memset(device, 0, sizeof(*device));

    kfree(device);
}

/*!
 * @brief: Destroy a device
 */
void destroy_device(device_t* device)
{
    if (device->obj->parent)
        device_unregister(device);

    /* Destroy the parent object */
    destroy_oss_obj(device->obj);
}

/*!
 * @brief: Try to bind @driver to @device
 */
kerror_t device_bind_driver(device_t* device, struct drv_manifest* driver)
{
    uint32_t idx = 0;
    drv_manifest_t** slot;

    mutex_lock(device->lock);

    do {
        slot = &device->drivers[idx++];
    } while (*slot && idx < DEVICE_DRIVER_LIMIT);

    /* Fill the slot */
    if (!(*slot))
        *slot = driver;

    mutex_unlock(device->lock);

    /* If idx == DEVICE_DRIVER_LIMIT then we could not find an empty slot */
    return (idx == DEVICE_DRIVER_LIMIT);
}

kerror_t device_unbind_driver(device_t* device, struct drv_manifest* driver)
{
    kerror_t error = -KERR_NOT_FOUND;

    mutex_lock(device->lock);

    for (uint32_t i = 0; i < DEVICE_DRIVER_LIMIT; i++) {
        if (device->drivers[i] != driver)
            continue;

        device->drivers[i] = nullptr;
        error = KERR_NONE;
        break;
    }

    mutex_unlock(device->lock);
    return error;
}

kerror_t device_clear_drivers(device_t* device)
{
    drv_manifest_t* c_driver;

    mutex_lock(device->lock);

    for (uint32_t i = 0; i < DEVICE_DRIVER_LIMIT; i++) {
        c_driver = device->drivers[i];

        (void)manifest_remove_dev(c_driver, device);
    }

    /* Clear the drivers list */
    memset(device->drivers, 0, sizeof(device->drivers));

    mutex_unlock(device->lock);
    return 0;
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

    error = device_get_group(dev, &dgroup);

    if (error)
        return error;

    return dev_group_remove_device(dgroup, dev);
}

/*!
 * @brief: Moves a device to a new devicegroup
 *
 * Takes the devices lock.
 * NOTE: It is important that this is done BEFORE binding a driver
 * to this device.
 */
int device_move(device_t* dev, struct dgroup* newgroup)
{
    int error;
    dgroup_t* oldgroup;

    if (!dev || !newgroup)
        return -KERR_INVAL;

    error = -1;

    mutex_lock(dev->lock);

    /* Check if the device has a driver attached */
    if (device_has_driver(dev))
        goto unlock_and_exit;

    error = device_get_group(dev, &oldgroup);

    if (error)
        goto unlock_and_exit;

    error = dev_group_remove_device(oldgroup, dev);

    if (error)
        goto unlock_and_exit;

    error = dev_group_add_device(newgroup, dev);

unlock_and_exit:
    mutex_unlock(dev->lock);
    return error;
}

int device_move_to_bus(device_t* dev, device_t* newbus)
{
    if (!newbus)
        return -KERR_INVAL;

    if (!device_is_bus(newbus))
        return -KERR_INVAL;

    return device_move(dev, newbus->bus_group);
}

static bool __device_itterate(oss_node_t* node, oss_obj_t* obj, void* arg)
{
    DEVICE_ITTERATE ittr;

    if (!arg)
        return false;

    ittr = arg;

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
    oss_obj_t* obj;
    device_t* ret;

    error = oss_resolve_obj(path, &obj);

    if (error || !obj)
        return nullptr;

    ret = oss_obj_get_device(obj);

    /* If the object is invalid we have to close it */
    if (!ret) {
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

/*!
 * @brief: Check if @dev has a driver attached
 */
bool device_has_driver(device_t* dev)
{
    for (uint32_t i = 0; i < DEVICE_DRIVER_LIMIT; i++)
        /* Driver, yes */
        if (dev->drivers[i])
            return true;

    return false;
}

kerror_t device_bind_acpi(device_t* dev, struct acpi_device* acpidev)
{
    if (dev->acpi_dev && acpidev)
        return -KERR_INVAL;

    mutex_lock(dev->lock);
    dev->acpi_dev = acpidev;
    mutex_unlock(dev->lock);

    return KERR_NONE;
}

kerror_t device_bind_pci(device_t* dev, struct pci_device* pcidev)
{
    if (dev->pci_dev && pcidev)
        return -KERR_INVAL;

    mutex_lock(dev->lock);
    dev->pci_dev = pcidev;
    mutex_unlock(dev->lock);

    return KERR_NONE;
}

/*!
 * @brief: Rename a device
 *
 * Caller should have taken the devices lock
 */
kerror_t device_rename(device_t* dev, const char* newname)
{
    kerror_t error;

    if (!dev || !newname)
        return -KERR_INVAL;

    /* Rename the object */
    error = oss_obj_rename(dev->obj, newname);

    /* This would be bad lololololol */
    if (error)
        return error;

    if (dev->name)
        kfree((void*)dev->name);

    /* Do our own rename */
    dev->name = strdup(newname);

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

int device_getinfo(device_t* dev, DEVINFO* binfo)
{
    device_ep_t* ep;

    ep = device_get_endpoint(dev, ENDPOINT_TYPE_GENERIC);

    if (!ep || !ep->impl.generic->f_get_devinfo)
        return -KERR_NODEV;

    return ep->impl.generic->f_get_devinfo(dev, binfo);
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
 * @brief: Enables the device @dev
 *
 * This does no immediate power state management, but the device driver may choose to implement
 * this in the generic device endpoint
 *
 * TODO: Make enabled/disabled state a atomic counter?
 */
int device_enable(device_t* dev)
{
    int error;
    struct device_endpoint* generic;

    if ((dev->flags & DEV_FLAG_ENABLED) == DEV_FLAG_ENABLED)
        return 0;

    mutex_lock(dev->lock);

    /* Set the device flag */
    dev->flags |= DEV_FLAG_ENABLED;

    error = 0;
    generic = device_get_endpoint(dev, ENDPOINT_TYPE_GENERIC);

    if (!generic || !generic->impl.generic || !generic->impl.generic->f_enable)
        goto unlock_and_exit;

    /* Call the device endpoint for a enable, if it has it */
    error = generic->impl.generic->f_enable(dev);

    /* Yikes */
    if (error)
        dev->flags &= ~DEV_FLAG_ENABLED;

unlock_and_exit:
    mutex_unlock(dev->lock);
    return error;
}

/*!
 * @brief: Disables the device @dev
 *
 * This does no immediate power state management, but the device driver may choose to implement
 * this in the generic device endpoint
 *
 * TODO: Make enabled/disabled state a atomic counter?
 */
int device_disable(device_t* dev)
{
    int error;
    struct device_endpoint* generic;

    /* Device already disabled? */
    if ((dev->flags & DEV_FLAG_ENABLED) != DEV_FLAG_ENABLED)
        return 0;

    mutex_lock(dev->lock);

    /* Set the device flag */
    dev->flags &= ~DEV_FLAG_ENABLED;

    error = 0;
    generic = device_get_endpoint(dev, ENDPOINT_TYPE_GENERIC);

    if (!generic || !generic->impl.generic || !generic->impl.generic->f_disable)
        goto unlock_and_exit;

    /* Call the device endpoint for a enable, if it has it */
    error = generic->impl.generic->f_disable(dev);

    /* Yikes v2 */
    if (error)
        dev->flags |= DEV_FLAG_ENABLED;

unlock_and_exit:
    mutex_unlock(dev->lock);
    return error;
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

    init_null_device(_device_node);

    /* Enumerate devices */
}

/*!
 * @brief: Initialize hardware stuff
 */
void init_hw()
{
    /* Initialized the ACPI core driver */
    // Comented until we implement actual system-wide ACPI integration
    init_acpi_core();

    /* Load the USB drivers on our system */
    // load_usb_hcds();

    ASSERT_MSG(load_external_driver("Root/System/inptcore.drv"), "Could not load input stuff");
}

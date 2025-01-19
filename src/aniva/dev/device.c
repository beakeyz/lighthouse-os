#include "device.h"
#include "dev/core.h"
#include "dev/ctl.h"
#include "dev/driver.h"
#include "dev/group.h"
#include "dev/misc/null.h"
#include "dev/usb/usb.h"
#include "libk/flow/error.h"
#include "lightos/api/device.h"
#include "lightos/api/objects.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/object.h"
#include "sync/mutex.h"
#include "system/acpi/acpi.h"
#include <libk/string.h>

static oss_object_t* _device_object;

static int __destroy_device(oss_object_t* device);

device_t* create_device(driver_t* parent, char* name, void* priv)
{
    return create_device_ex(parent, name, priv, DEVICE_CTYPE_SOFTDEV, NULL, NULL);
}

static oss_object_ops_t device_oss_ops = {
    .f_Destroy = __destroy_device,
};

/*!
 * @brief: Allocate memory for a device on @path
 *
 * TODO: create a seperate allocator that lives in dev/core.c for faster
 * device (de)allocation
 *
 * NOTE: this does not register a device to a driver, which means that it won't have a parent
 */
device_t* create_device_ex(struct driver* parent, char* name, void* priv, enum DEVICE_CTYPE type, uint32_t flags, device_ctl_node_t* ctl_list)
{
    int error;
    device_t* ret;

    if (!name)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->lock = create_mutex(NULL);
    ret->ep_lock = create_mutex(NULL);
    ret->obj = create_oss_object(name, OF_NO_DISCON, OT_DEVICE, &device_oss_ops, ret);
    ret->name = ret->obj->key;
    ret->drivers[0] = parent;
    ret->flags = flags;
    ret->private = priv;
    ret->type = type;
    ret->ctlmap = create_device_ctlmap(ret);

    if (ctl_list)
        device_impl_ctl_n(ret, parent, ctl_list);

    /* Make sure we register ourselves to the driver */
    if (parent)
        driver_add_dev(parent, ret);

    /* Execute the create control code if it's implemented */
    error = device_send_ctl(ret, DEVICE_CTLC_CREATE);

    /* Failed to create device on the driver side, return null */
    if (error && (error != -KERR_NOT_FOUND)) {
        destroy_device(ret);
        return nullptr;
    }

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
int __destroy_device(oss_object_t* object)
{
    device_t* device;

    if (!object || !object->private || object->type != OT_DEVICE)
        return -EINVAL;

    device = object->private;

    /* Let the system know that there was a driver removed */
    device_clear_drivers(device);

    destroy_mutex(device->lock);
    destroy_mutex(device->ep_lock);
    destroy_device_ctlmap(device->ctlmap);

    memset(device, 0, sizeof(*device));

    kfree(device);
    return 0;
}

/*!
 * @brief: Destroy a device
 */
void destroy_device(device_t* device)
{
    /* Send a destroy control code to the driver */
    (void)device_send_ctl(device, DEVICE_CTLC_DESTROY);

    /* Unregister the device */
    device_unregister(device);

    /* Close the parent object */
    oss_object_close(device->obj);
}

/*!
 * @brief: Try to bind @driver to @device
 */
kerror_t device_bind_driver(device_t* device, struct driver* driver)
{
    uint32_t idx = 0;
    driver_t** slot;

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

kerror_t device_unbind_driver(device_t* device, struct driver* driver)
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
    driver_t* c_driver;

    mutex_lock(device->lock);

    for (uint32_t i = 0; i < DEVICE_DRIVER_LIMIT; i++) {
        c_driver = device->drivers[i];

        (void)driver_remove_dev(c_driver, device);
    }

    /* Clear the drivers list */
    memset(device->drivers, 0, sizeof(device->drivers));

    mutex_unlock(device->lock);
    return 0;
}

/*!
 * @brief: Add a new dgroup to the device node
 */
int device_node_add_group(oss_object_t* node, dgroup_t* group)
{
    if (!group || !group->object)
        return -1;

    if (!node)
        node = _device_object;

    return oss_object_connect(node, group->object);
}

/*!
 * @brief: Registers a device to the device tree
 *
 * This assumes @group is already registered
 */
int device_register(device_t* dev, dgroup_t* group)
{
    error_t error;
    oss_object_t* node;

    node = _device_object;

    if (group)
        node = group->object;

    error = oss_object_connect(node, dev->obj);

    if (error)
        return error;

    /* Yes, do the thing lol */
    dev->group = group;

    return 0;
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

/*!
 * @brief: Open a device from oss
 *
 * We enforce devices to be attached to the device root
 */
device_t* device_open(const char* path)
{
    int error;
    oss_object_t* obj;

    error = oss_open_object(path, &obj);

    if (error || !obj)
        return nullptr;

    if (obj->private && obj->type == OT_DEVICE)
        return obj->private;

    /* Close the bastard again =( */
    oss_object_close(obj);

    return nullptr;
}

int device_close(device_t* dev)
{
    /* TODO: ... */
    return oss_object_close(dev->obj);
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
    error = oss_object_rename(dev->obj, newname);

    /* This would be bad lololololol */
    if (error)
        return error;

    /* Do our own rename */
    dev->name = dev->obj->key;

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
    if (!dev->group)
        return -ENOENT;

    *group_out = dev->group;
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
 * @brief: Implements a number of control codes after each other
 *
 * Fails if either one of the codes is a duplicate or any error occurs during implementation
 */
int device_impl_ctl_n(device_t* dev, struct driver* driver, device_ctl_node_t* ctl_list)
{
    int error;

    /* Check params before we lock shit */
    if (!dev || !ctl_list)
        return -KERR_INVAL;

    mutex_lock(dev->ep_lock);

    while (ctl_list->code) {

        /* Mask the rollover flag */
        device_ctlflags_mask_status(ctl_list->flags);

        error = device_map_ctl(dev->ctlmap, ctl_list->code, driver, ctl_list->ctl, ctl_list->flags);

        if (error)
            break;

        /* Next ctl */
        ctl_list++;
    }

    mutex_unlock(dev->ep_lock);
    return error;
}

/*!
 * @brief: Implements a single device control code for a driver
 */
int device_impl_ctl(device_t* dev, struct driver* driver, enum DEVICE_CTLC code, f_device_ctl_t impl, u16 flags)
{
    int error;

    /* Check params before we lock shit */
    if (!dev || !code || !impl)
        return -KERR_INVAL;

    /* Mask the rollover flag */
    device_ctlflags_mask_status(flags);

    mutex_lock(dev->ep_lock);

    error = device_map_ctl(dev->ctlmap, code, driver, impl, flags);

    mutex_unlock(dev->ep_lock);

    return error;
}

int device_unimpl_ctl(device_t* dev, enum DEVICE_CTLC code)
{
    int error;

    mutex_lock(dev->ep_lock);

    error = device_unmap_ctl(dev->ctlmap, code);

    mutex_unlock(dev->ep_lock);

    return error;
}

/*!
 * @brief: Sends a control code to a device
 *
 * Controlcodes are implemented by device drivers, so the control
 * will get redirected and handled to that driver
 */
int device_send_ctl(device_t* dev, enum DEVICE_CTLC code)
{
    return device_send_ctl_ex(dev, code, NULL, NULL, NULL);
}

/*!
 * @brief: ...
 */
int device_send_ctl_ex(device_t* dev, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t size)
{
    int error;

    if (!dev)
        return -EINVAL;

    mutex_lock(dev->ep_lock);

    error = device_ctl(dev->ctlmap, code, offset, buffer, size);

    mutex_unlock(dev->ep_lock);

    return error;
}

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
    return device_send_ctl_ex(dev, DEVICE_CTLC_READ, offset, buffer, size);
}

/*!
 * @brief: Write to a device
 *
 * Should only be implemented on device types where it makes sense. Write opperations should
 * be documented in such a way that there is no ambiguity
 */
int device_write(device_t* dev, void* buffer, uintptr_t offset, size_t size)
{
    return device_send_ctl_ex(dev, DEVICE_CTLC_WRITE, offset, buffer, size);
}

int device_getinfo(device_t* dev, DEVINFO* binfo)
{
    return device_send_ctl_ex(dev, DEVICE_CTLC_GETINFO, NULL, binfo, sizeof(*binfo));
}

int device_power_on(device_t* dev)
{
    kerror_t error;

    /* Send the poweron control */
    error = device_send_ctl(dev, DEVICE_CTLC_POWER_ON);

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
    return device_send_ctl(dev, DEVICE_CTLC_REMOVE);
}

/*!
 * @brief: ...
 */
int device_suspend(device_t* dev)
{
    return device_send_ctl(dev, DEVICE_CTLC_SUSPEND);
}

/*!
 * @brief: ...
 */
int device_resume(device_t* dev)
{
    return device_send_ctl(dev, DEVICE_CTLC_RESUME);
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

    if ((dev->flags & DEV_FLAG_ENABLED) == DEV_FLAG_ENABLED)
        return 0;

    mutex_lock(dev->lock);

    error = device_send_ctl(dev, DEVICE_CTLC_ENABLE);

    /* Yikes */
    if (!error)
        dev->flags |= DEV_FLAG_ENABLED;

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

    /* Device already disabled? */
    if ((dev->flags & DEV_FLAG_ENABLED) != DEV_FLAG_ENABLED)
        return 0;

    mutex_lock(dev->lock);

    error = device_send_ctl(dev, DEVICE_CTLC_DISABLE);

    /* Yikes v2 */
    if (error)
        dev->flags &= ~DEV_FLAG_ENABLED;

    mutex_unlock(dev->lock);
    return error;
}

void debug_devices()
{
    // device_for_each(NULL, _itter);

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
    oss_open_object(oss_get_default_rootobj_key(ORT_DEVICES), &_device_object);

    KLOG_DBG("A: 0x%p\n", _device_object);

    init_dgroups();
    KLOG_DBG("A: 0x%p\n", _device_object);

    init_null_device(_device_object);
    KLOG_DBG("A: 0x%p\n", _device_object);

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
    load_usb_hcds();
}

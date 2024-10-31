#include "volume.h"
#include "dev/device.h"
#include "dev/disk/device.h"
#include "dev/driver.h"
#include "dev/group.h"
#include "devices/device.h"
#include "devices/shared.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <oss/core.h>
#include <oss/node.h>

static dgroup_t* volumes_dgroup;
static dgroup_t* volume_devices_dgroup;
static u32 next_voldv_id;

#define VOLUME_NAME_FMT "V%d_%d"
#define VOLUME_DEVICE_NAME_FMT "VD%d"

volume_t* create_volume(volume_info_t* info, u32 flags)
{
    volume_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    if (info)
        ret->info = *info;

    ret->id = -1;
    ret->flags = flags;
    ret->lock = create_mutex(NULL);
    ret->ulock = create_mutex(NULL);

    return ret;
}

void destroy_volume(volume_t* volume)
{
    destroy_mutex(volume->lock);
    destroy_mutex(volume->ulock);

    /* Ensure we're not still registered */
    if (volume->parent)
        unregister_volume(volume);

    /* unregister_volume should set ->dev to nullptr, but maybe it fucked up? */
    if (volume->dev)
        destroy_device(volume->dev);

    kfree(volume);
}

/*!
 * @brief: Volume wrapper read function
 *
 * Makes sure the bounds of the volume are not exceeded on the volume device
 */
static int __volume_read(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    u64 read_end_offset;
    volume_t* vol;
    volume_device_t* voldev;

    if (!dev->private)
        return -KERR_INVAL;

    vol = dev->private;

    /* Check the offset range */
    if (vol->info.min_offset + offset > vol->info.max_offset)
        return -KERR_RANGE;

    if (!vol->parent)
        return -KERR_NODEV;

    voldev = vol->parent;
    read_end_offset = vol->info.min_offset + offset + size - 1;

    /* Correct the size overflow */
    if (read_end_offset > vol->info.max_offset)
        size = (vol->info.max_offset - (vol->info.min_offset + offset)) + 1;

    /* Do the read */
    return volume_dev_read(voldev, vol->info.min_offset + offset, buffer, size);
}

/*!
 * @brief: Volume wrapper write function
 *
 * Makes sure the bounds of the volume are not exceeded on the volume device
 */
static int __volume_write(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    u64 write_end_offset;
    volume_t* vol;
    volume_device_t* voldev;

    if (!dev->private)
        return -KERR_INVAL;

    vol = dev->private;

    /* Check the offset range */
    if (vol->info.min_offset + offset > vol->info.max_offset)
        return -KERR_RANGE;

    if (!vol->parent)
        return -KERR_NODEV;

    voldev = vol->parent;
    write_end_offset = vol->info.min_offset + offset + size - 1;

    /* Correct the size overflow */
    if (write_end_offset > vol->info.max_offset)
        size = (vol->info.max_offset - (vol->info.min_offset + offset)) + 1;

    /* Do the write */
    return volume_dev_write(voldev, vol->info.min_offset + offset, buffer, size);
}

static int __volume_flush(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    /* TODO */
    return 0;
}

/*!
 * @brief: Register a volume to a device
 *
 * Gives the volume its ID and it's own device.
 * This function is only called when a volume device is scanned for volumes
 */
int register_volume(volume_device_t* parent, volume_t* volume, volume_id_t id)
{
    char name_buffer[32] = { 0 };
    static device_ctl_node_t __volume_ctl_nodes[] = {
        DEVICE_CTL(DEVICE_CTLC_READ, __volume_read, NULL),
        DEVICE_CTL(DEVICE_CTLC_WRITE, __volume_write, NULL),
        DEVICE_CTL(DEVICE_CTLC_FLUSH, __volume_flush, NULL),
        DEVICE_CTL_END,
    };

    if (!parent || !volume)
        return -KERR_INVAL;

    /* Already registered */
    if (volume->dev)
        return -KERR_INVAL;

    /* Format the volumes name */
    sfmt(name_buffer, VOLUME_NAME_FMT, parent->id, id);

    volume->id = id;
    volume->parent = parent;
    volume->dev = create_device_ex(NULL, name_buffer, volume, DEVICE_CTYPE_OTHER, NULL, __volume_ctl_nodes);

    /* Just shove the volume at the beginning of the vector */
    volume->next = parent->vec_volumes;

    /* Update prev pointer */
    if (parent->vec_volumes)
        parent->vec_volumes->prev = volume;

    parent->vec_volumes = volume;
    parent->nr_volumes++;

    /* Add the volumes device to the group! */
    device_register(volume->dev, volumes_dgroup);

    return 0;
}

/*!
 * @brief: Forcefully remove a volume from it's volume device
 *
 * Should probably be avoided
 */
int unregister_volume(volume_t* volume)
{
    volume_device_t* parent;

    /* This can't be */
    if (!volume->dev)
        return -KERR_INVAL;

    /* No need to remove this volume from a parent (it's an orphan lmao) */
    if (!volume->parent)
        goto __destroy_device;

    parent = volume->parent;

    /* No actual volumes on this device, just kill it */
    if (!parent->vec_volumes)
        goto __destroy_device;

    /* Do some fixups */
    if (volume->prev)
        volume->prev->next = volume->next;
    if (volume->next)
        volume->next->prev = volume->prev;

    /* Do this thing */
    if (parent->vec_volumes->id == volume->id)
        parent->vec_volumes = volume->next;

    parent->nr_volumes--;

__destroy_device:
    destroy_device(volume->dev);

    /* Clear the volumes fields */
    volume->parent = nullptr;
    volume->next = nullptr;
    volume->prev = nullptr;
    volume->dev = nullptr;

    return 0;
}

/* Volume opperation routines */
size_t volume_read(volume_t* volume, uintptr_t offset, void* buffer, size_t size)
{
    if (!volume->dev)
        return 0;

    if (device_read(volume->dev, buffer, offset, size))
        return 0;

    return size;
}

size_t volume_write(volume_t* volume, uintptr_t offset, void* buffer, size_t size)
{
    if (!volume->dev)
        return 0;

    if (device_write(volume->dev, buffer, offset, size))
        return 0;

    return size;
}

int volume_flush(volume_t* volume)
{
    return device_send_ctl(volume->dev, DEVICE_CTLC_FLUSH);
}

int volume_resize(volume_t* volume, uintptr_t new_min_offset, uintptr_t new_max_offset)
{
    kernel_panic("TODO: volume_resize");
}
/* Completely removes a volume, also unregisters it and destroys it */
int volume_remove(volume_t* volume)
{
    kernel_panic("TODO: volume_remove");
}

static int __volume_dev_read(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    if (!dev || !dev->private)
        return -KERR_INVAL;

    return volume_dev_read(dev->private, offset, buffer, size);
}

static int __volume_dev_bread(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    if (!dev || !dev->private)
        return -KERR_INVAL;

    return volume_dev_bread(dev->private, offset, buffer, size);
}

static int __volume_dev_write(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    if (!dev || !dev->private)
        return -KERR_INVAL;

    return volume_dev_write(dev->private, offset, buffer, size);
}

static int __volume_dev_bwrite(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    if (!dev || !dev->private)
        return -KERR_INVAL;

    return volume_dev_bwrite(dev->private, offset, buffer, size);
}

static int __volume_dev_flush(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    if (!dev || !dev->private)
        return -KERR_INVAL;

    return volume_dev_flush(dev->private);
}

/*!
 * @brief: Add a volume device to %/VolumeDvs
 *
 * Gives the volume device it's ID and device object
 */
int register_volume_device(struct volume_device* volume_dev)
{
    char name_buffer[32] = { 0 };
    static device_ctl_node_t __volume_dev_ctl_nodes[] = {
        DEVICE_CTL(DEVICE_CTLC_READ, __volume_dev_read, NULL),
        DEVICE_CTL(DEVICE_CTLC_VOLUME_BREAD, __volume_dev_bread, NULL),
        DEVICE_CTL(DEVICE_CTLC_WRITE, __volume_dev_write, NULL),
        DEVICE_CTL(DEVICE_CTLC_VOLUME_BWRITE, __volume_dev_bwrite, NULL),
        DEVICE_CTL(DEVICE_CTLC_FLUSH, __volume_dev_flush, NULL),
        DEVICE_CTL_END,
    };

    /* Format the device name */
    sfmt(name_buffer, VOLUME_DEVICE_NAME_FMT, next_voldv_id);

    volume_dev->dev = create_device_ex(NULL, name_buffer, volume_dev, DEVICE_CTYPE_OTHER, NULL, __volume_dev_ctl_nodes);
    volume_dev->id = next_voldv_id;
    next_voldv_id++;

    return device_register(volume_dev->dev, volume_devices_dgroup);
}

int unregister_volume_device(struct volume_device* volume_dev)
{
    if (!volume_dev)
        return -KERR_INVAL;

    destroy_device(volume_dev->dev);
    volume_dev->dev = nullptr;
    return 0;
}

volume_device_t* volume_device_find(volume_id_t id)
{
    device_t* result = nullptr;
    char name_buffer[32] = { 0 };

    sfmt(name_buffer, VOLUME_DEVICE_NAME_FMT, id);

    /* Search this group for the device with this ID */
    if (dev_group_get_device(volumes_dgroup, name_buffer, &result) || !result)
        return nullptr;

    return result->private;
}

void init_root_ram_volume()
{
    volume_device_t* root_device;
    volume_t* root_ramdisk;
    volume_id_t device_id;

    device_id = 1;
    root_ramdisk = nullptr;
    root_device = volume_device_find(0);

    while (root_device && (root_device->flags & (VOLUME_DEV_FLAG_COMPRESSED | VOLUME_DEV_FLAG_MEMORY)) != (VOLUME_DEV_FLAG_COMPRESSED | VOLUME_DEV_FLAG_MEMORY))
        root_device = volume_device_find(device_id++);

    /* Fuck, we could not find a ram volume to mount =( */
    if (!root_device)
        return;

    /* We can use this ramdisk as a fallback to mount */
    root_ramdisk = root_device->vec_volumes;

    if (!root_ramdisk || oss_attach_fs(nullptr, FS_DEFAULT_ROOT_MP, "cramfs", root_ramdisk)) {
        kernel_panic("Could not find a root device to mount! TODO: fix");
    }
}

/*!
 * @brief: Initialize the volumes subsystem
 *
 * Organises volumes under %/Volumes and %/Dev/Volumes, where the former contains all volume objects and the latter contains any
 * volume carrier devices, which usually back any data associated with a volume.
 */
void init_volumes()
{
    oss_node_t* root_node;

    /* Find the rootnode */
    ASSERT_MSG(oss_resolve_node("%", &root_node) == 0, "Failed to find rootnode to init volumes");

    next_voldv_id = 0;
    volumes_dgroup = register_dev_group(DGROUP_TYPE_MISC, "Volumes", NULL, root_node);
    volume_devices_dgroup = register_dev_group(DGROUP_TYPE_MISC, "VolumeDvs", NULL, NULL);
}

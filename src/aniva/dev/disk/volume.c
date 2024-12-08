#include "volume.h"
#include "dev/device.h"
#include "dev/disk/device.h"
#include "dev/driver.h"
#include "dev/group.h"
#include "lightos/dev/shared.h"
#include "fs/core.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "lightos/volume/shared.h"
#include <oss/core.h>
#include <oss/node.h>

static dgroup_t* volumes_dgroup;
static dgroup_t* volume_devices_dgroup;
static u32 next_voldv_id;

#define VOLUME_NAME_FMT "V%d.%d"
#define VOLUME_DEVICE_NAME_FMT "VD%d"

volume_t* create_volume(volume_info_t* info, u32 flags)
{
    volume_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    /* Copy over the info */
    memcpy(&ret->info, info, sizeof(*info));

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
 * @brief: Volume wrapper read function
 *
 * Makes sure the bounds of the volume are not exceeded on the volume device
 */
static int __volume_bread(device_t* dev, driver_t* drv, uintptr_t blk, void* buffer, size_t count)
{
    u64 read_end_offset;
    u64 offset;
    size_t size;
    volume_t* vol;
    volume_device_t* voldev;

    if (!dev->private)
        return -KERR_INVAL;

    vol = dev->private;

    /* Invalid =( */
    if (!vol || !vol->info.logical_sector_size)
        return -KERR_INVAL;

    if (!vol->parent)
        return -KERR_NODEV;

    voldev = vol->parent;
    /* Compute offset and size */
    offset = vol->info.logical_sector_size * blk;
    size = vol->info.logical_sector_size * count;

    /* Check the offset range */
    if ((vol->info.min_offset + offset) > vol->info.max_offset)
        return -KERR_RANGE;

    read_end_offset = vol->info.min_offset + offset + size - 1;

    /* Correct the size overflow */
    if (read_end_offset > vol->info.max_offset)
        count = ((vol->info.max_offset - (vol->info.min_offset + offset)) + 1) / vol->info.logical_sector_size;

    /* Do the write */
    return volume_dev_bread(voldev, volume_get_min_blk(&vol->info) + blk, buffer, count);
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

/*!
 * @brief: Volume wrapper write function
 *
 * Makes sure the bounds of the volume are not exceeded on the volume device
 */
static int __volume_bwrite(device_t* dev, driver_t* drv, uintptr_t blk, void* buffer, size_t count)
{
    u64 write_end_offset;
    u64 offset;
    size_t size;
    volume_t* vol;
    volume_device_t* voldev;

    if (!dev->private)
        return -KERR_INVAL;

    vol = dev->private;

    /* Invalid =( */
    if (!vol || !vol->info.logical_sector_size)
        return -KERR_INVAL;

    /* Compute offset and size */
    offset = vol->info.logical_sector_size * blk;
    size = count * vol->info.logical_sector_size;

    /* Check the offset range */
    if (vol->info.min_offset + offset > vol->info.max_offset)
        return -KERR_RANGE;

    if (!vol->parent)
        return -KERR_NODEV;

    voldev = vol->parent;
    write_end_offset = vol->info.min_offset + offset + size - 1;

    /* Correct the size overflow */
    if (write_end_offset > vol->info.max_offset)
        count = ((vol->info.max_offset - (vol->info.min_offset + offset)) + 1) / vol->info.logical_sector_size;

    /* Do the write */
    return volume_dev_bwrite(voldev, volume_get_min_blk(&vol->info) + blk, buffer, count);
}

static int __volume_flush(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    /* TODO */
    return 0;
}

static int __generic_getinfo(device_t* dev, volume_info_t* src_info, DEVINFO* p_devinfo, size_t devinfo_sz)
{
    volume_info_t* vinfo_dest;

    if (!dev || !src_info)
        return -KERR_INVAL;

    /* Check for valid buffer params */
    if (!p_devinfo || devinfo_sz != sizeof(DEVINFO))
        return -KERR_INVAL;

    /* Grab vinfo */
    vinfo_dest = nullptr;

    /* Check if we have a buffer for dev_specific stuff */
    if (p_devinfo->dev_specific_info && p_devinfo->dev_specific_size == sizeof(*vinfo_dest))
        if (!kmem_validate_ptr(get_current_proc(), (vaddr_t)p_devinfo->dev_specific_info, sizeof(*vinfo_dest)))
            vinfo_dest = (volume_info_t*)p_devinfo->dev_specific_info;

    p_devinfo->class = dev->class;
    p_devinfo->subclass = dev->subclass;
    p_devinfo->deviceid = dev->device_id;
    p_devinfo->vendorid = dev->vendor_id;
    p_devinfo->ctype = dev->type;

    if (vinfo_dest)
        memcpy(vinfo_dest, src_info, sizeof(*vinfo_dest));

    return 0;
}

/*!
 * @brief: Gather volume info
 */
static int __volume_getinfo(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    volume_t* volume;

    if (!dev || !dev->private)
        return -KERR_NODEV;

    volume = (volume_t*)dev->private;

    return __generic_getinfo(dev, &volume->info, buffer, size);
}

/*!
 * @brief: Register a volume to a device
 *
 * Gives the volume its ID and it's own device.
 * This function is only called when a volume device is scanned for volumes, or when custom
 * volumes are added to a device
 */
int register_volume(volume_device_t* parent, volume_t* volume, volume_id_t id)
{
    char name_buffer[32] = { 0 };
    static device_ctl_node_t __volume_ctl_nodes[] = {
        DEVICE_CTL(DEVICE_CTLC_READ, __volume_read, NULL),
        DEVICE_CTL(DEVICE_CTLC_WRITE, __volume_write, NULL),
        DEVICE_CTL(DEVICE_CTLC_VOLUME_BREAD, __volume_bread, NULL),
        DEVICE_CTL(DEVICE_CTLC_VOLUME_BWRITE, __volume_bwrite, NULL),
        DEVICE_CTL(DEVICE_CTLC_FLUSH, __volume_flush, NULL),
        DEVICE_CTL(DEVICE_CTLC_GETINFO, __volume_getinfo, NULL),
        DEVICE_CTL_END,
    };

    if (!parent || !volume)
        return -KERR_INVAL;

    /* Just make sure that the parent device is registered at this point */
    ASSERT_MSG(parent->dev, "Tried to register a volume to an unregistered volume device");

    /* Already registered */
    if (volume->dev)
        return -KERR_INVAL;

    /* Format the volumes name */
    sfmt(name_buffer, VOLUME_NAME_FMT, parent->id, id);

    volume->id = id;
    volume->parent = parent;
    volume->dev = create_device_ex(NULL, name_buffer, volume, DEVICE_CTYPE_OTHER, NULL, __volume_ctl_nodes);

    volume->info.volume_id = id;
    volume->info.device_id = parent->id;

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

    if (__volume_read(volume->dev, nullptr, offset, buffer, size))
        return 0;

    return size;
}

size_t volume_write(volume_t* volume, uintptr_t offset, void* buffer, size_t size)
{
    if (!volume->dev)
        return 0;

    if (__volume_write(volume->dev, nullptr, offset, buffer, size))
        return 0;

    return size;
}

size_t volume_bread(volume_t* volume, uintptr_t block, void* buffer, size_t nr_blks)
{
    if (!volume || !buffer || !nr_blks)
        return 0;

    if (__volume_bread(volume->dev, nullptr, block, buffer, nr_blks))
        return 0;

    return nr_blks * volume->info.logical_sector_size;
}

size_t volume_bwrite(volume_t* volume, uintptr_t block, void* buffer, size_t nr_blks)
{
    if (!volume || !buffer || !nr_blks)
        return 0;

    if (__volume_bwrite(volume->dev, nullptr, block, buffer, nr_blks))
        return 0;

    return nr_blks * volume->info.logical_sector_size;
}

int volume_flush(volume_t* volume)
{
    return __volume_flush(volume->dev, NULL, NULL, NULL, NULL);
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
 * @brief: Gather volume info
 */
static int __volume_dev_getinfo(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    volume_device_t* vdev;

    if (!dev || !dev->private)
        return -KERR_INVAL;

    vdev = (volume_device_t*)dev->private;

    return __generic_getinfo(dev, &vdev->info, buffer, size);
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
        DEVICE_CTL(DEVICE_CTLC_GETINFO, __volume_dev_getinfo, NULL),
        DEVICE_CTL_END,
    };

    /* Format the device name */
    sfmt(name_buffer, VOLUME_DEVICE_NAME_FMT, next_voldv_id);

    KLOG_DBG("Creating volume device: %s\n", name_buffer);

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
    if (dev_group_get_device(volume_devices_dgroup, name_buffer, &result) || !result)
        return nullptr;

    return result->private;
}

/*!
 * @brief: Check the currently mounted root filesystem for our system files
 *
 * We need to check for a few file(types) to be present on the system:
 * - The kernel file
 * - The kernel BASE variable file, created by the installer
 * - The base softdrivers that are required for userspace
 * - Other resource files for the kernel
 * - ect. (TODO)
 */
static bool verify_mount_root()
{
    oss_obj_t* scan_obj;

    /*
     * Try to find kterm in the system directory. If it exists, that means there at least
     * a somewhat functional system installed on this disk
     *
     * FIXME: Put the locations of essential drivers in the profile variables of BASE
     */
    if (oss_resolve_obj_rel(nullptr, FS_DEFAULT_ROOT_MP "/System/kterm.drv", &scan_obj))
        return false;

    if (!scan_obj)
        return false;

    /*
     * We could find the system file!
     * TODO: look for more files that only one O.o
     */
    oss_obj_close(scan_obj);
    return true;
}

/*!
 * @brief: Check all the available filesystems to see if one is compatible with @device
 *
 * Also performs checks on the files inside the filesystem, if the mount call succeeds
 */
static bool try_mount_root(volume_t* device)
{
    int error;
    bool verify_result;
    oss_node_t* c_node;
    static const char* filesystems[] = {
        "fat32",
        //"ext2",
    };
    static const uint32_t filesystems_count = sizeof(filesystems) / sizeof(*filesystems);

    KLOG_DBG("Trying to mount filesystem to volume: %s\n", device->dev->name);

    for (uint32_t i = 0; i < filesystems_count; i++) {
        const char* fs = filesystems[i];

        error = oss_attach_fs(nullptr, FS_DEFAULT_ROOT_MP, fs, device);

        /* Successful mount? try and verify the mount */
        if (error)
            continue;

        KLOG_DBG("Checking if the mount did what we want...\n");
        verify_result = verify_mount_root();

        /* Did we succeed??? =DD */
        if (verify_result)
            break;

        KLOG_DBG("NOpe, go next\n");
        /* Reset the result */
        error = -1;

        // vfs_unmount(VFS_ROOT_ID"/"VFS_DEFAULT_ROOT_MP);
        /* Detach the node first */
        oss_detach_fs(FS_DEFAULT_ROOT_MP, &c_node);

        /* Then destroy it */
        destroy_oss_node(c_node);
    }

    /* Failed to scan for filesystem */
    if (error)
        return false;

    return true;
}

static int _set_bootdevice(volume_t* device)
{
    int error = -KERR_INVAL;
    const char* path;
    const char* sysvar_names[] = {
        BOOT_DEVICE_VARKEY,
        BOOT_DEVICE_NAME_VARKEY,
        BOOT_DEVICE_SIZE_VARKEY,
    };
    u64 sysvar_values[] = {
        0, // Will get set later
        (u64)device->dev->name,
        device->info.max_offset * device->info.logical_sector_size,
    };
    sysvar_t* sysvar;
    user_profile_t* profile = get_admin_profile();

    if (!device)
        return -1;

    path = oss_obj_get_fullpath(device->dev->obj);

    if (!path)
        return -1;

    sysvar_values[0] = (u64)path;

    for (u32 i = 0; i < 3; i++) {
        sysvar = sysvar_get(profile->node, sysvar_names[i]);
        error = -KERR_NOT_FOUND;

        if (!sysvar)
            goto free_and_exit;

        error = -KERR_INVAL;

        if (!sysvar_write(sysvar, sysvar_values[i]))
            goto free_and_exit;
    }

    error = 0;
free_and_exit:
    kfree((void*)path);
    return error;
}

void init_root_volume()
{
    /*
     * Init probing for the root device
     */
    const char* initrd_mp;
    volume_t* cur_part;
    volume_device_t* root_device;
    volume_id_t device_index;
    oss_node_t* initial_ramfs_node;

    volume_t* root_ramdisk;

    device_index = 1;
    root_ramdisk = nullptr;
    root_device = volume_device_find(0);

    KLOG_DBG("Trying to find root volume...\n");

    oss_detach_fs(FS_DEFAULT_ROOT_MP, &initial_ramfs_node);

    if (initial_ramfs_node)
        destroy_oss_node(initial_ramfs_node);

    while (root_device) {

        cur_part = root_device->vec_volumes;

        if ((root_device->flags & VOLUME_DEV_FLAG_MEMORY) == VOLUME_DEV_FLAG_MEMORY && cur_part && (cur_part->flags & VOLUME_FLAG_CRAM) == VOLUME_FLAG_CRAM) {
            /* We can use this ramdisk as a fallback to mount */
            root_ramdisk = cur_part;
            goto cycle_next;
        }

        /*
         * NOTE: we use this as a last resort. If there is no mention of a root device anywhere,
         * we bruteforce every partition and check if it contains a valid FAT32 filesystem. If
         * it does, we check if it contains the files that make up our boot filesystem and if it does, we simply take
         * that device and mark as our root.
         */
        while (cur_part) {

            /* Try to mount a filesystem and scan for aniva system files */
            if (try_mount_root(cur_part))
                break;

            cur_part = cur_part->next;
        }

        if (cur_part) {
            KLOG_DBG("Found a rootdevice: %s -> %s\n", root_device->dev->name, cur_part->dev->name);
            break;
        }

    cycle_next:
        root_device = volume_device_find(device_index++);
        cur_part = nullptr;
    }

    initrd_mp = FS_INITRD_MP;

    /* If there was no root device, use the initrd as a rootdevice */
    if (!cur_part)
        initrd_mp = FS_DEFAULT_ROOT_MP;
    else
        _set_bootdevice(cur_part);

    if (!root_ramdisk || oss_attach_fs(nullptr, initrd_mp, "cramfs", root_ramdisk))
        kernel_panic("Could not find a root device to mount! TODO: fix");
    else {
        _set_bootdevice(root_ramdisk);

        KLOG_DBG("Could not find root volume. Remounting ramfs (at: %%/%s/)\n", initrd_mp);
    }
}

void init_root_ram_volume()
{
    volume_device_t* root_device;
    volume_t* root_ramdisk;
    volume_id_t device_id;

    device_id = 1;
    root_ramdisk = nullptr;
    root_device = volume_device_find(0);

    while (root_device && (root_device->flags & (VOLUME_DEV_FLAG_MEMORY)) != (VOLUME_DEV_FLAG_MEMORY) && root_device->nr_volumes != 1) {
        KLOG_DBG("root: %s\n", root_device->dev->name);
        root_device = volume_device_find(device_id++);
    }

    /* Fuck, we could not find a ram volume to mount =( */
    if (!root_device)
        kernel_panic("Failed to find ramdisk");

    /* We can use this ramdisk as a fallback to mount */
    root_ramdisk = root_device->vec_volumes;

    if (!root_ramdisk || oss_attach_fs(nullptr, FS_DEFAULT_ROOT_MP, "cramfs", root_ramdisk))
        kernel_panic("Could not find a root device to mount! TODO: fix");
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
    volume_devices_dgroup = register_dev_group(DGROUP_TYPE_MISC, "Volume", NULL, NULL);
}

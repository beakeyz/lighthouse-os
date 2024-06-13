#include "generic.h"
#include "dev/core.h"
#include "dev/disk/partition/gpt.h"
#include "dev/disk/partition/mbr.h"
#include "dev/driver.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "fs/core.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "oss/core.h"
#include "oss/obj.h"
#include "ramdisk.h"
#include <dev/device.h>
#include <dev/manifest.h>
#include <libk/string.h>
#include <oss/node.h>
#include <sync/mutex.h>

#define MAX_EFFECTIVE_SECTORCOUNT 512
#define DISK_DEVICE_NAME_PREFIX "drive"

// static char* s_root_dev_name;
// static char s_root_dev_name_buffer[64];
static mutex_t* _gdisk_lock;

/* This is so dumb */
struct gdisk_store_entry {
    disk_dev_t* dev;
    struct gdisk_store_entry* next;
};

static struct gdisk_store_entry* s_gdisks;
static struct gdisk_store_entry* s_last_gdisk;
static size_t _c_drive_idx;

int disk_core_init();
int disk_core_exit();

/*!
 * @brief Generate a new disk uid (duid)
 *
 * NOTE: caller should have the s_gdisk_lock held
 */
static disk_uid_t generate_new_disk_uid()
{

    if (!mutex_is_locked_by_current_thread(_gdisk_lock))
        return -1;

    struct gdisk_store_entry* entry = s_last_gdisk;

    disk_uid_t ret;

    if (!entry) {
        ret = 0;
        goto exit;
    }

    ret = entry->dev->m_uid + 1;

    // Label is here for eventual locking / allocation cleaning
exit:;

    return ret;
}

/*!
 * @brief Get a gdisk store entry for a certain device
 *
 * Nothing to add here...
 */
struct gdisk_store_entry** get_store_entry(disk_dev_t* device)
{

    struct gdisk_store_entry** entry = &s_gdisks;

    for (; (*entry); entry = &(*entry)->next) {
        if ((*entry)->dev == device) {
            return entry;
        }
    }

    return entry;
}

/*!
 * @brief Create a gdisk store entry for @device
 *
 * NOTE: the caller of this should have the s_gdisk_lock held, because of the
 * generate_new_disk_uid call...
 */
static struct gdisk_store_entry* create_gdisk_store_entry(disk_dev_t* device)
{
    struct gdisk_store_entry* ret;

    if (!device)
        return nullptr;

    ret = kmalloc(sizeof(struct gdisk_store_entry));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(struct gdisk_store_entry));

    ret->dev = device;
    ret->next = nullptr;

    device->m_uid = generate_new_disk_uid();

    return ret;
}

/*!
 * @brief Remove a gdisk store entry
 *
 * Also makes sure that the underlying device gets its uuid removed
 * (Since -1 is invalid)
 */
static void destroy_gdisk_store_entry(struct gdisk_store_entry* entry)
{

    if (!entry)
        return;

    if (entry->dev)
        entry->dev->m_uid = (disk_uid_t)-1;

    kfree(entry);
}

/*!
 * @brief Create a partitioned disk device
 *
 * Nothing to add here...
 */
partitioned_disk_dev_t* create_partitioned_disk_dev(disk_dev_t* parent, char* name, uint64_t start, uint64_t end, uint32_t flags)
{
    partitioned_disk_dev_t* ret;

    if (!name)
        return nullptr;

    ret = kmalloc(sizeof(partitioned_disk_dev_t));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(partitioned_disk_dev_t));

    ret->m_parent = parent;
    ret->m_name = name;
    ret->m_flags = flags;

    ret->m_start_lba = start;
    ret->m_end_lba = end;

    /* TODO: check for range */
    ret->m_block_size = parent->m_logical_sector_size;
    ret->m_next = nullptr;

    return ret;
}

/*!
 * @brief Free memory used for a partitioned disk device
 *
 * Nothing to add here...
 */
void destroy_partitioned_disk_dev(partitioned_disk_dev_t* dev)
{
    kfree(dev);
}

/*!
 * @brief Attach a partitioned disk device to a regular disk device
 *
 * Nothing to add here...
 */
void attach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev)
{

    partitioned_disk_dev_t** device;

    /*
     * This code is stupid
     */
    for (device = &generic->m_devs; *device; device = &(*device)->m_next) {
        if (!(*device)) {
            break;
        }
    }

    if (!(*device)) {
        *device = dev;
        generic->m_partitioned_dev_count++;
    }
}

/*!
 * @brief Remove a partitioned disk device from its regular device
 *
 * Nothing to add here...
 */
void detach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev)
{

    if (!dev || !generic)
        return;

    ASSERT_MSG(generic->m_partitioned_dev_count, "Tried to detach partitioned disk device when the dev count of the generic disk device was already zero!");

    partitioned_disk_dev_t** previous = nullptr;
    partitioned_disk_dev_t** device;

    for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
        if ((*device) == dev) {
            if (previous) {
                (*previous)->m_next = (*device)->m_next;
            }

            generic->m_partitioned_dev_count--;
            break;
        }

        previous = device;
    }
}

/*!
 * @brief Scan the generic disk device to find a pdd for @block
 *
 * Nothing to add here...
 */
partitioned_disk_dev_t** fetch_designated_device_for_block(disk_dev_t* generic, uintptr_t block)
{
    partitioned_disk_dev_t** device;

    for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
        uintptr_t start_lba = (*device)->m_start_lba;
        uintptr_t end_lba = (*device)->m_end_lba;

        // Check if the block is contained in this partition
        if (block >= start_lba && block <= end_lba) {
            return device;
        }
    }

    return nullptr;
}

int generic_disk_opperation(disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset)
{
    return -1;
}

kerror_t register_gdisk_dev(disk_dev_t* device)
{
    struct gdisk_store_entry** entry;

    if (!device)
        return -1;

    entry = get_store_entry(device);

    /* We need to assert that get_store_entry returned an empty entry */
    if (*entry)
        return -1;

    mutex_lock(_gdisk_lock);

    *entry = create_gdisk_store_entry(device);

    /* Then update the tail pointer */
    s_last_gdisk = *entry;

    mutex_unlock(_gdisk_lock);

    return (0);
}

/*!
 * @brief Remove a disk device from our store
 *
 * Nothing to add here...
 */
kerror_t unregister_gdisk_dev(disk_dev_t* device)
{
    struct gdisk_store_entry** entry;
    struct gdisk_store_entry** previous_entry;

    if (!device)
        return -1;

    mutex_lock(_gdisk_lock);

    previous_entry = nullptr;
    entry = &s_gdisks;

    /* First we detacht the entry */
    for (; *entry; entry = &(*entry)->next) {
        /* Found the right entry */
        if ((*entry)->dev == device) {
            if (previous_entry && *previous_entry) {
                /* Detach */
                (*previous_entry)->next = (*entry)->next;

                /* Fixup the pointer to the last entry if we remove that entry */
                if (!(*entry)->next) {
                    s_last_gdisk = *previous_entry;
                }
            } else {
                /* This is the first entry */
                s_gdisks = (*entry)->next;
            }
            break;
        }

        previous_entry = entry;
    }

    /* No entry found =/ */
    if (!*entry) {
        mutex_unlock(_gdisk_lock);
        return -1;
    }

    /* If we remove the only entry, update s_last_gdisk */
    if ((!previous_entry || !*previous_entry) && !(*entry)->next && s_last_gdisk)
        s_last_gdisk = nullptr;

    struct gdisk_store_entry* current = (*entry)->next;

    /* Fixup the uids of the devices after this one */
    while (current) {
        current->dev->m_uid--;

        current = current->next;
    }

    /* Object not needed anymore, destroy it */
    destroy_gdisk_store_entry(*entry);

    mutex_unlock(_gdisk_lock);

    return (0);
}

/*!
 * @brief Find a disk device by its uid
 *
 * Nothing to add here...
 */
disk_dev_t* find_gdisk_device(disk_uid_t uid)
{

    disk_uid_t i = 0;

    for (struct gdisk_store_entry* entry = s_gdisks; entry; entry = entry->next) {
        if (i == uid) {
            if (entry->dev->m_uid == uid)
                return entry->dev;

            /* Shit, something didn't match */
            break;
        }
        i++;
    }

    /* FIXME: what can we do to fix the indexing issues at this point? */

    return nullptr;
}

/*!
 * @brief Register a gdisk device with a specific uid
 *
 * Nothing to add here...
 */
kerror_t register_gdisk_dev_with_uid(disk_dev_t* device, disk_uid_t uid)
{

    if (!device)
        return -1;

    mutex_lock(_gdisk_lock);

    if (uid == 0) {
        struct gdisk_store_entry* entry = create_gdisk_store_entry(device);
        /* Match the uid assignment in the create call */
        entry->dev->m_uid = uid;

        /* Place at the front */
        entry->next = s_gdisks;
        s_gdisks = entry;

        entry = s_gdisks->next;

        ASSERT_MSG(entry->dev->m_uid == 0, "gdisk store seems unsorted! (uid of first entry != 0)");

        while (entry) {
            entry->dev->m_uid++;
            entry = entry->next;
        }
    } else {

        struct gdisk_store_entry* entry = s_gdisks;
        struct gdisk_store_entry* previous = nullptr;

        for (; entry; entry = entry->next) {

            /* Found the entry we need to 'shift forward' */
            if (entry->dev->m_uid == uid)
                break;

            previous = entry;
        }

        struct gdisk_store_entry* new_entry = create_gdisk_store_entry(device);
        new_entry->dev->m_uid = uid;

        /* Emplace */
        previous->next = new_entry;
        new_entry->next = entry;

        /* Fixup the uids */
        while (entry) {

            entry->dev->m_uid++;

            entry = entry->next;
        }
    }

    mutex_unlock(_gdisk_lock);

    return (0);
}

int read_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset)
{
    uintptr_t block;

    if (!dev || !dev->m_parent)
        return -KERR_INVAL;

    block = offset / dev->m_parent->m_logical_sector_size;

    if (block < dev->m_start_lba || block > dev->m_end_lba)
        return -KERR_INVAL;

    return device_read(dev->m_parent->m_dev, buffer, offset, size);
}

int write_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset)
{
    kernel_panic("TODO: implement write_sync_partitioned");
    return 0;
}

/*
 * A bunch of block ops
 * We need to make these functions less fucking weird lmao
 */

int read_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t count, uintptr_t block)
{
    device_ep_t* ep;
    uintptr_t abs_block;
    int result = -1;

    if (!dev || !dev->m_parent)
        return result;

    abs_block = dev->m_start_lba + block;
    ep = device_get_endpoint(dev->m_parent->m_dev, ENDPOINT_TYPE_DISK);

    if (abs_block < dev->m_start_lba || abs_block > dev->m_end_lba)
        return result;

    if (ep->impl.disk->f_bread)
        result = ep->impl.disk->f_bread(dev->m_parent->m_dev, buffer, abs_block, count);

    return result;
}

int write_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t count, uintptr_t block)
{
    device_ep_t* ep;
    uintptr_t abs_block;
    int result = -1;

    if (!dev || !dev->m_parent)
        return result;

    abs_block = dev->m_start_lba + block;
    ep = device_get_endpoint(dev->m_parent->m_dev, ENDPOINT_TYPE_DISK);

    if (abs_block < dev->m_start_lba || abs_block > dev->m_end_lba)
        return result;

    if (ep->impl.disk->f_bwrite)
        result = ep->impl.disk->f_bwrite(dev->m_parent->m_dev, buffer, abs_block, count);

    return result;
}

int pd_bread(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn)
{
    return read_sync_partitioned_blocks(dev, buffer, 1, blockn);
}

int pd_bwrite(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn)
{
    return write_sync_partitioned_blocks(dev, buffer, 1, blockn);
}

int pd_set_blocksize(partitioned_disk_dev_t* dev, uint32_t blksize)
{
    if (blksize > SMALL_PAGE_SIZE || blksize < 512 || !POWER_OF_2(blksize))
        return -1;

    if (blksize < dev->m_parent->m_logical_sector_size)
        return -1;

    dev->m_block_size = blksize;

    return 0;
}

disk_dev_t* create_generic_ramdev(size_t size)
{
    uintptr_t start_addr;

    /* Align the size */
    size = ALIGN_UP(size, SMALL_PAGE_SIZE);

    /* Allocate a block */
    ASSERT(!__kmem_kernel_alloc_range((void**)&start_addr, size, KMEM_CUSTOMFLAG_CREATE_USER, KMEM_FLAG_WRITABLE));

    /* Create a ramdev */
    return create_generic_ramdev_at(start_addr, size);
}

/*
 * This is okay since ramdisks have a 'blocksize' of one byte lmao
 * We need to implement this endpoint to classify as 'diskdevice'
 */
static struct device_disk_endpoint _rd_disk_ep = {
    .f_bread = ramdisk_read,
    .f_bwrite = ramdisk_write,
};

static struct device_generic_endpoint _rd_gen_ep = {
    .f_read = ramdisk_read,
    .f_write = ramdisk_write,
};

static device_ep_t _rd_eps[] = {
    { ENDPOINT_TYPE_GENERIC, sizeof(_rd_gen_ep), { &_rd_gen_ep } },
    { ENDPOINT_TYPE_DISK, sizeof(_rd_disk_ep), { &_rd_disk_ep } },
    {
        NULL,
    },
};

/*
 * We leave it to the caller to map the addressspace
 * TODO: maybe we could pass an addressspace object that
 * enforces mappings...
 */
disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size)
{
    /* Trolle xD */
    disk_dev_t* dev;
    partitioned_disk_dev_t* partdev;
    const size_t ramdisk_blksize = sizeof(uint8_t);

    if (!size)
        return nullptr;

    /* NOTE: ideally, we want this to be the first 'drive' we create, so we claim the first entry */
    dev = create_generic_disk(NULL, "ramdisk", NULL, _rd_eps);

    if (!dev)
        return nullptr;

    dev->m_logical_sector_size = ramdisk_blksize;
    dev->m_physical_sector_size = ramdisk_blksize;
    dev->m_max_blk = size;
    dev->m_flags |= GDISKDEV_FLAG_RAM;

    /* A ramdisk can't use async IO by definition */
    partdev = create_partitioned_disk_dev(dev, "rampart0", address, address + size, PD_FLAG_ONLY_SYNC);

    attach_partitioned_disk_device(dev, partdev);

    /* Attach the ramdisk to the root of the device tree */
    device_register(dev->m_dev, NULL);
    return dev;
}

bool destroy_generic_ramdev(disk_dev_t* device)
{

    /* If we get passed a faulty ramdevice, just die */
    if (!device || (device->m_flags & GDISKDEV_FLAG_RAM) == 0 || !device->m_devs || device->m_devs->m_next)
        return false;

    const uintptr_t start_addr = device->m_devs->m_start_lba;
    const uintptr_t end_addr = device->m_devs->m_end_lba;

    const size_t ramdev_size = end_addr - start_addr;

    kerror_t error = __kmem_kernel_dealloc(start_addr, ramdev_size);

    if (error)
        return false;

    /* We should only have one partition, so this works */
    destroy_partitioned_disk_dev(device->m_devs);

    kfree(device);

    return true;
}

partitioned_disk_dev_t* find_partition_at(disk_dev_t* diskdev, uint32_t idx)
{

    partitioned_disk_dev_t* ret;

    if (!diskdev || idx >= diskdev->m_partitioned_dev_count)
        return nullptr;

    ret = diskdev->m_devs;

    /* TODO: find out if this is faster than normal linear scan */
    while (idx) {
        if (!ret)
            return nullptr;

        if (idx % 2 == 0) {
            ret = ret->m_next->m_next;
            idx -= 2;
        } else {
            ret = ret->m_next;
            idx--;
        }
    }

    return ret;
}

static int __diskdev_populate_mbr_part_table(disk_dev_t* dev)
{
    mbr_table_t* mbr_table = create_mbr_table(dev, 0);

    /* No valid mbr prob, yikes */
    if (!mbr_table)
        return -1;

    dev->mbr_table = mbr_table;
    dev->m_partition_type = PART_TYPE_MBR;
    dev->m_partitioned_dev_count = 0;

    FOREACH(i, mbr_table->partitions)
    {
        mbr_entry_t* entry = i->data;

        partitioned_disk_dev_t* partitioned_device = create_partitioned_disk_dev(dev, (char*)mbr_partition_names[dev->m_partitioned_dev_count], entry->offset, (entry->offset + entry->length) - 1, PD_FLAG_ONLY_SYNC);

        attach_partitioned_disk_device(dev, partitioned_device);
    }

    return 0;
}

static int __diskdev_populate_gpt_part_table(disk_dev_t* dev)
{
    /* Lay out partitions (FIXME: should we really do this here?) */
    gpt_table_t* gpt_table = create_gpt_table(dev);

    if (!gpt_table)
        return -1;

    /* Cache the gpt table */
    dev->gpt_table = gpt_table;
    dev->m_partition_type = PART_TYPE_GPT;
    dev->m_partitioned_dev_count = 0;

    /*
     * TODO ?: should we put every partitioned device in the vfs?
     * Neh, that whould create the need to implement file opperations
     * on devices
     */

    /* Attatch the partition devices to the generic disk device */
    FOREACH(i, gpt_table->m_partitions)
    {
        gpt_partition_t* part = i->data;

        partitioned_disk_dev_t* partitioned_device = create_partitioned_disk_dev(dev, part->m_type.m_name, part->m_start_lba, part->m_end_lba, PD_FLAG_ONLY_SYNC);

        attach_partitioned_disk_device(dev, partitioned_device);
    }
    return 0;
}

/*!
 * @brief Grab the correct partitioning sceme and populate the partitioned device link
 *
 * Nothing to add here...
 */
int diskdev_populate_partition_table(disk_dev_t* dev)
{
    int error;

    /* Just in case */
    dev->m_partition_type = PART_TYPE_NONE;

    error = __diskdev_populate_gpt_part_table(dev);

    /* On an error, go next */
    if (!error)
        return 0;

    error = __diskdev_populate_mbr_part_table(dev);

    if (!error)
        return 0;

    return -1;
}

int ramdisk_read(device_t* _dev, void* buffer, disk_offset_t offset, size_t size)
{
    disk_dev_t* device;

    if (!_dev)
        return -1;

    device = _dev->private;

    if (!device || !device->m_devs || !device->m_physical_sector_size)
        return -1;

    if (!buffer || !size)
        return -1;

    /* A ramdisk device always only contains one 'partition' */
    if (device->m_devs->m_next)
        return -1;

    const uintptr_t start_addr = device->m_devs->m_start_lba;
    const uintptr_t end_addr = device->m_devs->m_end_lba;

    const uintptr_t read_addr = start_addr + offset;

    if (read_addr > end_addr || read_addr + size > end_addr)
        return -1;

    memcpy(buffer, (void*)read_addr, size);

    return 0;
}

int ramdisk_write(device_t* device, void* buffer, disk_offset_t offset, size_t size)
{
    kernel_panic("TODO: implement ramdisk_write");
}

static inline char* _construct_dev_name()
{
    char* ret;
    size_t len;
    char* prefix = DISK_DEVICE_NAME_PREFIX;
    char* suffix = (char*)to_string(_c_drive_idx++);

    len = strlen(prefix) + strlen(suffix) + 1;

    ret = kmalloc(len);

    if (!ret)
        return nullptr;

    memset(ret, 0, len);
    concat(prefix, suffix, ret);

    return ret;
}

/*!
 * @brief: Creates a generic disk device
 *
 * Also attaches it to the core disk driver
 */
disk_dev_t* create_generic_disk(struct drv_manifest* parent, char* name, void* private, device_ep_t* eps)
{
    disk_dev_t* ret = nullptr;
    char* dev_name = nullptr;

    /* Duplicate name so we can simply free dev_name when we're done */
    if (name)
        dev_name = strdup(name);
    else
        dev_name = _construct_dev_name();

    if (!dev_name)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        goto dealloc_and_exit;

    memset(ret, 0, sizeof(*ret));

    ret->m_priv = private;
    ret->m_dev = create_device_ex(parent, dev_name, ret, NULL, eps);

    if (!ret || !device_has_endpoint(ret->m_dev, ENDPOINT_TYPE_DISK))
        goto dealloc_and_exit;

    ret->m_ops = device_get_endpoint(ret->m_dev, ENDPOINT_TYPE_DISK)->impl.disk;

    kfree((void*)dev_name);
    return ret;

dealloc_and_exit:
    if (dev_name)
        kfree((void*)dev_name);
    if (ret)
        kfree(ret);
    return nullptr;
}

/*!
 * @brief Deallocate any structures or caches owned by the disk device
 *
 * Nothing to add here...
 */
void destroy_generic_disk(disk_dev_t* device)
{
    partitioned_disk_dev_t* itter;
    partitioned_disk_dev_t* current;

    switch (device->m_partition_type) {
    case PART_TYPE_GPT:
        destroy_gpt_table(device->gpt_table);
        break;
    case PART_TYPE_MBR:
        // destroy_mbr_table(device->mbr_table);
        break;
    case PART_TYPE_NONE:
        break;
    }

    itter = device->m_devs;

    while (itter) {
        /* Cache next before we destroy it */
        current = itter;
        itter = itter->m_next;

        destroy_partitioned_disk_dev(current);
    }

    /* Drop our device ref */
    device_close(device->m_dev);

    kfree(device);
}

/*!
 * @brief: Set the most effectieve sector size of a diskdevice
 */
void disk_set_effective_sector_count(disk_dev_t* dev, uint32_t count)
{
    ASSERT_MSG(dev->m_logical_sector_size, "Tried to set the effective sectorcount before the logical sectorsize was known");

    if (count > MAX_EFFECTIVE_SECTORCOUNT)
        count = MAX_EFFECTIVE_SECTORCOUNT;
    else if (!count)
        count = 1;

    dev->m_max_sector_transfer_count = count;
    dev->m_effective_sector_size = count * dev->m_logical_sector_size;
}

/*!
 * @brief: Initialize the aniva disk subsystem
 *
 * This is the part of aniva that is responsible for managing (Async) access to external
 * containers of semi-permanent data (Hard disks, SSDs, NVME drives, USB drives,
 * floppies (yuck), ect.). This should happen in sync and ideally distributed.
 *
 * Major TODO: refactor this a bit so device access is less ambiguous (There currently are
 * a fuck ton of different ways to read bytes off of a disk, and they all pretty much do
 * the same thing -_-).
 */
void init_gdisk_dev()
{
    _c_drive_idx = 0;
    _gdisk_lock = create_mutex(0);
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
static bool try_mount_root(partitioned_disk_dev_t* device)
{
    int error;
    bool verify_result;
    oss_node_t* c_node;
    const char* filesystems[] = {
        "fat32",
        //"ext2",
    };
    const uint32_t filesystems_count = sizeof(filesystems) / sizeof(*filesystems);

    for (uint32_t i = 0; i < filesystems_count; i++) {
        const char* fs = filesystems[i];

        error = oss_attach_fs(nullptr, FS_DEFAULT_ROOT_MP, fs, device);

        /* Successful mount? try and verify the mount */
        if (error)
            continue;

        verify_result = verify_mount_root();

        /* Did we succeed??? =DD */
        if (verify_result)
            break;

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

void init_root_device_probing()
{
    /*
     * Init probing for the root device
     */
    bool found_root_device;
    const char* initrd_mp;
    disk_dev_t* root_device;
    disk_uid_t device_index;
    oss_node_t* initial_ramfs_node;

    partitioned_disk_dev_t* root_ramdisk;

    device_index = 1;
    found_root_device = false;
    root_ramdisk = nullptr;
    root_device = find_gdisk_device(0);

    oss_detach_fs(FS_DEFAULT_ROOT_MP, &initial_ramfs_node);

    if (initial_ramfs_node)
        destroy_oss_node(initial_ramfs_node);

    while (root_device) {

        partitioned_disk_dev_t* part = root_device->m_devs;

        if ((root_device->m_flags & GDISKDEV_FLAG_RAM) == GDISKDEV_FLAG_RAM) {
            /* We can use this ramdisk as a fallback to mount */
            root_ramdisk = part;
            goto cycle_next;
        }

        /*
         * NOTE: we use this as a last resort. If there is no mention of a root device anywhere,
         * we bruteforce every partition and check if it contains a valid FAT32 filesystem. If
         * it does, we check if it contains the files that make up our boot filesystem and if it does, we simply take
         * that device and mark as our root.
         */
        while (part && !found_root_device) {

            /* Try to mount a filesystem and scan for aniva system files */
            if ((found_root_device = try_mount_root(part)))
                break;

            part = part->m_next;
        }

    cycle_next:
        root_device = find_gdisk_device(device_index++);
    }

    initrd_mp = FS_INITRD_MP;

    /* If there was no root device, use the initrd as a rootdevice */
    if (!found_root_device)
        initrd_mp = FS_DEFAULT_ROOT_MP;

    if (!root_ramdisk || oss_attach_fs(nullptr, initrd_mp, "cramfs", root_ramdisk)) {
        kernel_panic("Could not find a root device to mount! TODO: fix");
    }
}

/*!
 * @brief: Fetch the ramdisk and attach it at Root
 */
void init_root_ramdev()
{
    disk_dev_t* root_device;
    disk_uid_t device_index;
    partitioned_disk_dev_t* root_ramdisk;

    device_index = 1;
    root_ramdisk = nullptr;
    root_device = find_gdisk_device(0);

    while (root_device && (root_device->m_flags & GDISKDEV_FLAG_RAM) != GDISKDEV_FLAG_RAM)
        root_device = find_gdisk_device(device_index++);

    /* We can use this ramdisk as a fallback to mount */
    root_ramdisk = root_device->m_devs;

    if (!root_ramdisk || oss_attach_fs(nullptr, FS_DEFAULT_ROOT_MP, "cramfs", root_ramdisk)) {
        kernel_panic("Could not find a root device to mount! TODO: fix");
    }
}

bool gdisk_is_valid(disk_dev_t* device)
{
    return (
        device && device->m_devs && device->m_dev->name && device->m_max_blk);
}

/*
 * TODO: pass driver messages through to the correct driver, given
 * a generic disk device / partitioned disk device
 */
uint64_t disk_core_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
    disk_uid_t uid;
    disk_dev_t* device;

    switch (code) {
    case DISK_DCC_GET_DEVICE: {
        if (!buffer || size != sizeof(disk_uid_t*) || !out_buffer || out_size != sizeof(disk_dev_t*))
            return DRV_STAT_INVAL;

        uid = *(disk_uid_t*)buffer;
        device = find_gdisk_device(uid);

        if (!device)
            return DRV_STAT_INVAL;

        memcpy(out_buffer, device, sizeof(*device));
        break;
    }
    }
    return DRV_STAT_OK;
}

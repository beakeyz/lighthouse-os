#include "ramdisk.h"
#include "dev/disk/device.h"
#include "dev/disk/volume.h"
#include "dev/driver.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "lightos/volume/shared.h"

static int __ramdisk_read(device_t* dev, driver_t* driver, u64 offset, void* buffer, size_t bsize)
{
    u64 start_addr;
    volume_device_t* volume;

    if (!dev || !buffer || !bsize)
        return -KERR_INVAL;

    volume = dev->private;

    if (!volume)
        return -KERR_INVAL;

    start_addr = volume->info.min_offset;

    /* Check offset range */
    if (start_addr + offset > volume->info.max_offset)
        return -KERR_RANGE;

    /* Check size range and truncate */
    if ((start_addr + offset + bsize) > volume->info.max_offset)
        bsize -= (start_addr + offset + bsize) - volume->info.max_offset;

    memcpy(buffer, (void*)start_addr + offset, bsize);

    return 0;
}

static int __ramdisk_write(device_t* dev, driver_t* driver, u64 offset, void* buffer, size_t bsize)
{
    u64 start_addr;
    volume_device_t* volume;

    if (!dev || !buffer || !bsize)
        return -KERR_INVAL;

    volume = dev->private;

    if (!volume)
        return -KERR_INVAL;

    start_addr = volume->info.min_offset;

    /* Check offset range */
    if (start_addr + offset > volume->info.max_offset)
        return -KERR_RANGE;

    /* Check size range and truncate */
    if ((start_addr + offset + bsize) > volume->info.max_offset)
        bsize -= (start_addr + offset + bsize) - volume->info.max_offset;

    memcpy((void*)start_addr + offset, buffer, bsize);

    return 0;
}

static int __ramdisk_flush(device_t* dev, driver_t* driver, u64 offset, void* buffer, size_t bsize)
{
    /* Don't do shit */
    return 0;
}

static int __ramdisk_getinfo(device_t* dev, driver_t* driver, u64 offset, void* buffer, size_t bsize)
{
    volume_device_t* volume;

    if (!dev || !buffer || !bsize)
        return -KERR_INVAL;

    volume = dev->private;

    if (!volume)
        return -KERR_INVAL;

    /* Clear the buffer */
    memset(buffer, 0, bsize);

    /* TODO: do shit */
    return 0;
}

static volume_dev_ops_t __ramdisk_ops = {
    .f_read = __ramdisk_read,
    .f_bread = __ramdisk_read,
    .f_write = __ramdisk_write,
    .f_bwrite = __ramdisk_write,
    .f_flush = __ramdisk_flush,
    .f_getinfo = __ramdisk_getinfo,
    .f_ata_ident = nullptr,
};

volume_device_t* create_generic_ramdev(size_t size)
{
    void* range;
    volume_device_t* ret;

    /* Try to allocate the ramdisk range */
    if (kmem_kernel_alloc_range(&range, size, NULL, KMEM_FLAG_WRITABLE))
        return nullptr;

    /* Try to create a ramdisk at this location */
    ret = create_generic_ramdev_at((uintptr_t)range, size);

    /* Ramdisk creation failed, delete this region */
    if (!ret)
        kmem_kernel_dealloc((u64)range, size);

    return ret;
}

volume_device_t* create_generic_ramdev_at(uintptr_t address, size_t size)
{
    volume_t* volume;
    volume_device_t* ret;
    volume_info_t info;

    info = (volume_info_t) {
        .min_offset = address,
        .max_offset = address + size,
        .logical_sector_size = 1,
        .physical_sector_size = 1,
        .max_transfer_sector_nr = 512,
        .type = VOLUME_TYPE_MEMORY,
        .firmware_rev = { 0x6969, 0x0000, 0x0000, 0x0420 },
        .label = "Ramdisk =)",
    };

    ret = create_volume_device(&info, &__ramdisk_ops, VOLUME_DEV_FLAG_MEMORY, nullptr);

    if (!ret)
        return nullptr;

    ASSERT_MSG(register_volume_device(ret) == 0, "Failed to register ramdisk volume device!");

    /* Create a cram volume */
    volume = create_volume(&info, VOLUME_FLAG_CRAM);

    if (!volume) {
        destroy_volume_device(ret);
        return nullptr;
    }

    /* Add this sucker, there shouldn't be anything going wrong here */
    ASSERT_MSG(register_volume(ret, volume, 0) == 0, "Failed to register volume to ramdisk volume device!");

    return ret;
}

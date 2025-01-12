#include "device.h"
#include "dev/disk/partition/gpt.h"
#include "dev/disk/volume.h"
#include "mem/heap.h"
#include "lightos/api/volume.h"
#include <dev/disk/partition/mbr.h>
#include <libk/string.h>

volume_device_t* create_volume_device(volume_info_t* info, volume_dev_ops_t* ops, u32 flags, void* private)
{
    volume_device_t* ret;

    if (!ops)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->ops = ops;
    ret->id = -1;
    ret->flags = flags;
    ret->private = private;

    /* Only copy the info if we have it */
    if (info)
        ret->info = *info;

    return ret;
}

void destroy_volume_device(volume_device_t* volume_dev)
{
    kfree(volume_dev);
}

static int __populate_mbr_part_table(volume_device_t* dev)
{
    mbr_table_t* mbr_table = create_mbr_table(dev, 0);

    /* No valid mbr prob, yikes */
    if (!mbr_table)
        return -1;

    /* Inform the device of it's partition type */
    dev->mbr_table = mbr_table;
    dev->info.type = VOLUME_TYPE_MBR;
    dev->nr_volumes = 0;

    FOREACH(i, mbr_table->partitions)
    {
        volume_info_t volume_info = dev->info;
        mbr_entry_t* entry = i->data;

        /* Copy the volume label */
        memcpy(volume_info.label, (char*)mbr_partition_names[dev->nr_volumes], sizeof(mbr_partition_names[dev->nr_volumes]));

        /* Get these blocks */
        volume_info.min_offset = entry->offset;
        volume_info.max_offset = entry->offset + entry->length - 1;

        /* Create a volume with the correct info */
        volume_t* volume = create_volume(&volume_info, NULL);

        if (!volume)
            continue;

        register_volume(dev, volume, dev->nr_volumes);
    }

    return 0;
}

static int __populate_gpt_part_table(volume_device_t* dev)
{
    /* Lay out partitions (FIXME: should we really do this here?) */
    gpt_table_t* gpt_table = create_gpt_table(dev);

    if (!gpt_table)
        return -1;

    /* Cache the gpt table */
    dev->gpt_table = gpt_table;
    dev->info.type = VOLUME_TYPE_GPT;
    dev->nr_volumes = 0;

    /*
     * TODO ?: should we put every partitioned device in the vfs?
     * Neh, that whould create the need to implement file opperations
     * on devices
     */
    volume_info_t volume_info = { 0 };

    /* Attatch the partition devices to the generic disk device */
    FOREACH(i, gpt_table->m_partitions)
    {
        gpt_partition_t* part = i->data;

        /* Copy the devices info block */
        memcpy(&volume_info, &dev->info, sizeof(dev->info));

        /* Copy the volume label */
        memcpy(volume_info.label, part->m_type.m_name, sizeof(part->m_type.m_name));

        /* Set the boundries */
        volume_info.min_offset = part->m_start_lba * volume_info.logical_sector_size;
        volume_info.max_offset = part->m_end_lba * volume_info.logical_sector_size;

        // partitioned_volume_device_t* partitioned_device = create_partitioned_disk_dev(dev, part->m_type.m_name, part->m_start_lba, part->m_end_lba, PD_FLAG_ONLY_SYNC);
        volume_t* volume = create_volume(&volume_info, NULL);

        if (!volume)
            continue;

        register_volume(dev, volume, dev->nr_volumes);
    }
    return 0;
}

int volume_dev_clear_volumes(volume_device_t* device)
{
    volume_t *walker, *next;

    if (!device)
        return -KERR_INVAL;

    walker = device->vec_volumes;

    /* Walk the devices volumes */
    while (walker) {
        next = walker->next;

        /* Destroy this volume */
        destroy_volume(walker);

        walker = next;
    }

    /* Ensure these fields are zeroed */
    device->vec_volumes = nullptr;
    device->nr_volumes = 0;
    return 0;
}

/*!
 * @brief Grab the correct partitioning sceme and populate the partitioned device link
 *
 * TODO: Do some locking of this stuff
 * Every volume should have it's own set of locks, which need to be taken by this function in order to be able to
 * clear and (re)populate the volumes of the device
 */
int volume_dev_populate_volumes(volume_device_t* dev)
{
    int error;

    /* If we already know the partition type, don't do shit */
    if (dev->info.type != VOLUME_TYPE_UNKNOWN)
        return -1;

    /* If this device already has volumes, clear them out and redo them */
    if (dev->nr_volumes || dev->vec_volumes)
        volume_dev_clear_volumes(dev);

    /* Just in case */
    dev->info.type = VOLUME_TYPE_UNKNOWN;

    error = __populate_gpt_part_table(dev);

    /* On an error, go next */
    if (!error)
        return 0;

    error = __populate_mbr_part_table(dev);

    if (!error)
        return 0;

    return -1;
}

/*!
 * @brief: Sets the volume info for a volume device
 *
 * NOTE: This is a function with hidden functionality, since it also tries to populate the
 * devices volumes with the partitions, if pinfo->type is set to VOLUME_TYPE_UNKNOWN
 */
int volume_dev_set_info(volume_device_t* device, volume_info_t* pinfo)
{
    if (!pinfo)
        return -KERR_INVAL;

    device->info = *pinfo;

    /* Add the volumes of this device */
    volume_dev_populate_volumes(device);

    return 0;
}

int volume_dev_read(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size)
{
    if (!volume_dev || !volume_dev->ops)
        return -KERR_INVAL;

    if (!volume_dev->ops->f_read)
        return -KERR_INVAL;

    return volume_dev->ops->f_read(volume_dev->dev, NULL, offset, buffer, size);
}

int volume_dev_bread(volume_device_t* volume_dev, u64 block, void* buffer, size_t nr_blocks)
{
    if (!volume_dev || !volume_dev->ops)
        return -KERR_INVAL;

    if (!volume_dev->ops->f_bread)
        return -KERR_INVAL;

    return volume_dev->ops->f_bread(volume_dev->dev, NULL, block, buffer, nr_blocks);
}

int volume_dev_write(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size)
{
    if (!volume_dev || !volume_dev->ops)
        return -KERR_INVAL;

    if (!volume_dev->ops->f_write)
        return -KERR_INVAL;

    return volume_dev->ops->f_write(volume_dev->dev, NULL, offset, buffer, size);
}

int volume_dev_bwrite(volume_device_t* volume_dev, u64 block, void* buffer, size_t nr_blocks)
{
    if (!volume_dev || !volume_dev->ops)
        return -KERR_INVAL;

    if (!volume_dev->ops->f_bwrite)
        return -KERR_INVAL;

    return volume_dev->ops->f_bwrite(volume_dev->dev, NULL, block, buffer, nr_blocks);
}

int volume_dev_flush(volume_device_t* volume_dev)
{
    if (!volume_dev || !volume_dev->ops)
        return -KERR_INVAL;

    if (!volume_dev->ops->f_bread)
        return -KERR_INVAL;

    return volume_dev->ops->f_flush(volume_dev->dev, NULL, NULL, nullptr, NULL);
}

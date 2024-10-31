#ifndef __LIGHTOS_VOLUMEIO_SHARED_H__
#define __LIGHTOS_VOLUMEIO_SHARED_H__

#include <stdint.h>

typedef int32_t volume_id_t;

#define VOLUME_ID_INVAL ((volume_id_t)(-1))

typedef enum VOLUME_TYPE {
    /* Guid partition sceme */
    VOLUME_TYPE_GPT,
    /* Master boot record */
    VOLUME_TYPE_MBR,
    /* Extended mbr */
    VOLUME_TYPE_EBR,
    /* We have no clue */
    VOLUME_TYPE_UNKNOWN,
    /* No partition sceme, just raw disk data I guess */
    VOLUME_TYPE_UNPARTITIONED,
    /* This volume has not yet been allocated to a purpose */
    VOLUME_TYPE_FREE_SPACE,
    /* This is a memory-backed volume */
    VOLUME_TYPE_MEMORY,
} VOLUME_TYPE_t;

typedef struct volume_info {
    /* ID of this volume */
    volume_id_t volume_id;
    /* ID of this volumes parent device */
    volume_id_t device_id;

    uint16_t firmware_rev[4];

    /* Boundry info */
    uintptr_t max_offset;
    uintptr_t min_offset;

    /* Blocksizes */
    uint32_t logical_sector_size;
    uint32_t physical_sector_size;
    uint32_t max_transfer_sector_nr;

    /* What type of volume is this */
    enum VOLUME_TYPE type;

    /* Volume label */
    char label[80];
} volume_info_t;

#define ___ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

static inline size_t volume_get_max_blk(volume_info_t* info)
{
    /* Don't want a devide by 0 exception *clown* */
    if (!info || !info->logical_sector_size)
        return 0;
    return ___ALIGN_DOWN(info->max_offset, info->logical_sector_size) / info->logical_sector_size;
}

static inline size_t volume_get_length(volume_info_t* info)
{
    if (!info)
        return 0;
    return info->max_offset - info->min_offset + 1;
}

#endif // !__LIGHTOS_VOLUMEIO_SHARED_H__

#ifndef __LIGHTOS_VOLUMEIO_SHARED_H__
#define __LIGHTOS_VOLUMEIO_SHARED_H__

#include <stdint.h>

typedef uint32_t volume_id_t;

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
    uintptr_t max_blk;
    uintptr_t min_blk;

    /* Blocksizes */
    uint32_t logical_sector_size;
    uint32_t physical_sector_size;
    uint32_t max_transfer_sector_nr;

    /* What type of volume is this */
    enum VOLUME_TYPE type;

    /* Volume label */
    char label[64];
} volume_info_t;

#endif // !__LIGHTOS_VOLUMEIO_SHARED_H__

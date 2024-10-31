#ifndef __ANIVA_DISK_VOLUME_H__
#define __ANIVA_DISK_VOLUME_H__

#include "dev/device.h"
#include "volumeio/shared.h"

struct volume_device;

#define VOLUME_FLAG_READONLY 0x00000001

/*
 * A generic volume object
 *
 * Volumes are implemented as inherent softdevices
 */
typedef struct volume {
    device_t* dev;

    /* ID that is used to generate a name for the volumes device */
    u32 id;
    u32 flags;

    /* Volume info block telling us some physical and practical properties */
    volume_info_t info;
    /* This volumes parent device */
    struct volume_device* parent;

    /* Pointer to the next volume, if this volume is managed by a volume device */
    struct volume* next;
} volume_t;

volume_t* create_volume(volume_id_t parent_id, volume_info_t* info, u32 flags);
void destroy_volume(volume_t* volume);

int register_volume(volume_t* volume);
int unregister_volume(volume_t* volume);

/* Volume opperation routines */
size_t volume_read(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
size_t volume_write(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
int volume_flush(volume_t* volume);
int volume_resize(volume_t* volume, uintptr_t new_min_blk, uintptr_t new_max_blk);
/* Completely removes a volume, also unregisters it and destroys it */
int volume_remove(volume_t* volume);

int register_volume_device(struct volume_device* volume_dev);
int unregister_volume_device(struct volume_device* volume_dev);

void init_volumes();

#endif // !__ANIVA_DISK_VOLUME_H__

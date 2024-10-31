#ifndef __ANIVA_DISK_VOLUME_H__
#define __ANIVA_DISK_VOLUME_H__

#include "dev/device.h"
#include "dev/disk/device.h"
#include "sync/mutex.h"
#include "volumeio/shared.h"

struct volume_device;

#define VOLUME_FLAG_READONLY 0x00000001
#define VOLUME_FLAG_NO_ASYNC 0x00000002

/*
 * A generic volume object
 *
 * Volumes are implemented as inherent softdevices
 */
typedef struct volume {
    device_t* dev;

    /* Generic volume lock. Taken by anyone who wishes to do ANYTHING
     * with this volume. When a process opens a handle to a volume, other
     * processes can't open this volume anymore, since that might lead to
     * weird stuff. We do however want the kernel to be able to alter the
     * volumes, when needed (like a hotplug event, or some other weird thing).
     * In this case, in order to prevent weird desyncs, we need to be able to
     * warn any users of this volume, that is has been altered (or even removed)
     */
    mutex_t* lock;
    mutex_t* ulock;

    /* ID that is used to generate a name for the volumes device */
    volume_id_t id;
    u32 flags;

    /* Volume info block telling us some physical and practical properties */
    volume_info_t info;
    /* This volumes parent device */
    struct volume_device* parent;

    /* Pointer to the next volume, if this volume is managed by a volume device */
    struct volume* next;
    struct volume* prev;
} volume_t;

volume_t* create_volume(volume_info_t* info, u32 flags);
void destroy_volume(volume_t* volume);

int register_volume(volume_device_t* parent, volume_t* volume, volume_id_t id);
int unregister_volume(volume_t* volume);

/* Volume opperation routines */
size_t volume_read(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
size_t volume_write(volume_t* volume, uintptr_t offset, void* buffer, size_t size);
int volume_flush(volume_t* volume);
int volume_resize(volume_t* volume, uintptr_t new_min_offset, uintptr_t new_max_offset);
/* Completely removes a volume, also unregisters it and destroys it */
int volume_remove(volume_t* volume);

int register_volume_device(struct volume_device* volume_dev);
int unregister_volume_device(struct volume_device* volume_dev);

volume_device_t* volume_device_find(volume_id_t id);

void init_root_ram_volume();
void init_volumes();

#endif // !__ANIVA_DISK_VOLUME_H__

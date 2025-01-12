#ifndef __LIGHTOS_VOLUMEIO_H__
#define __LIGHTOS_VOLUMEIO_H__

#include <lightos/handle.h>
#include <lightos/api/volume.h>

/* Common typedefs for using volumes */
typedef handle_t volume_hndl_t;
typedef HANDLE VOLUME_HNDL;

/*
 * Public structure for representing volume information
 *
 * TODO: Create a set of functions to work with this bastard
 */
typedef struct lightos_volume {
    VOLUME_HNDL handle;
    volume_id_t id;

    /* Physical information about the volume */
    size_t min_offset;
    size_t max_offset;
    uint32_t logical_sector_size;
    uint32_t physical_sector_size;

    /* Logical info */
    const char* volume_label;
    volume_id_t device_id;
} lightos_volume_t;

VOLUME_HNDL open_volume(const char* path, uint32_t flags, enum HNDL_MODE mode);
void close_volume(VOLUME_HNDL handle);

/* Functions to interface with volumes on a low level */
uint64_t volume_read(VOLUME_HNDL volume, uintptr_t offset, void* buffer, size_t size);
uint64_t volume_write(VOLUME_HNDL volume, uintptr_t offset, void* buffer, size_t size);
int volume_flush(VOLUME_HNDL volume);

int volume_get(VOLUME_HNDL handle, lightos_volume_t* pvolume);
void lightos_volume_clean(lightos_volume_t* lvolume);

#endif // !__LIGHTOS_VOLUMEIO_H__

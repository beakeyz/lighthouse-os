#include "lightos/api/device.h"
#include "lightos/api/handle.h"
#include "lightos/api/volume.h"
#include "lightos/handle.h"
#include "string.h"
#include "volumeio.h"
#include <lightos/dev/device.h>
#include <lightos/lightos.h>
#include <stdlib.h>

VOLUME_HNDL open_volume(const char* path, uint32_t flags, enum HNDL_MODE mode)
{
    VOLUME_HNDL handle;

    /* Volumes are a type of device */
    handle = open_handle(path, HNDL_TYPE_DEVICE, flags, mode);

    /* Check if we even recieved a valid handle */
    if (handle_verify(handle))
        return handle;

    return handle;
}

void close_volume(VOLUME_HNDL handle)
{
    volume_flush(handle);

    /* TODO: Volume specific cleaning */
    close_handle(handle);
}

/* Functions to interface with volumes on a low level */
uint64_t volume_read(VOLUME_HNDL volume, uintptr_t offset, void* buffer, size_t size)
{
    exit_noimpl("volume_read");
    return 0;
}

uint64_t volume_write(VOLUME_HNDL volume, uintptr_t offset, void* buffer, size_t size)
{
    exit_noimpl("volume_write");
    return 0;
}

int volume_flush(VOLUME_HNDL volume)
{
    exit_noimpl("volume_flush");

    return 0;
}

int volume_get(VOLUME_HNDL handle, lightos_volume_t* pvolume)
{
    volume_info_t volume_info = { 0 };
    DEVINFO devinfo = { 0 };

    /* Specify the location of the volume info */
    devinfo.dev_specific_size = sizeof(volume_info_t);
    devinfo.dev_specific_info = &volume_info;

    exit_noimpl("volume_get");

    pvolume->handle = handle;
    /* Dangerous */
    pvolume->volume_label = strdup(volume_info.label);

    /* Physical info */
    pvolume->max_offset = volume_info.max_offset;
    pvolume->min_offset = volume_info.min_offset;
    pvolume->logical_sector_size = volume_info.logical_sector_size;
    pvolume->physical_sector_size = volume_info.physical_sector_size;

    pvolume->id = volume_info.volume_id;
    pvolume->device_id = volume_info.device_id;

    return 0;
}

void lightos_volume_clean(lightos_volume_t* lvolume)
{
    free((void*)lvolume->volume_label);
}

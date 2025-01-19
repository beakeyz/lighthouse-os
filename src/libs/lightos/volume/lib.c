#include "lightos/api/device.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/api/volume.h"
#include "lightos/object.h"
#include "string.h"
#include "volumeio.h"
#include <lightos/dev/device.h>
#include <lightos/lightos.h>
#include <stdlib.h>

Object open_volume(const char* path, uint32_t flags, enum HNDL_MODE mode)
{
    Object object;

    object = OpenObject(path, flags, mode);

    /* Invalid type. Just close the object */
    if (object.type != OT_DEVICE)
        CloseObject(&object);

    return object;
}

void close_volume(Object* volume)
{
    volume_flush(volume->handle);

    /* TODO: Volume specific cleaning */
    CloseObject(volume);
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

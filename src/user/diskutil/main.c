#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include <errno.h>
#include <lightos/proc/cmdline.h>
#include <stdio.h>
#include <lightos/volume/volumeio.h>

/* Parameters */
static const char target_volume[256] = { 0 };
static const char target_mode[8] = { 0 };
static uint64_t target_offset = 0;
static size_t target_size = 0;

/* Static program data */
static VOLUME_HNDL volume;

static inline void __parse_target_mode(uint32_t* pflags, enum HNDL_MODE* pmode, bool* do_info)
{
    char c;

    /* Set defaults */
    *pflags = 0;
    *pmode = HNDL_MODE_NORMAL;
    *do_info = false;

    for (uint32_t i = 0; i < sizeof(target_mode); i++) {
        c = target_mode[i];

        switch (c) {
        case 'r':
        case 'R':
            *pflags |= HNDL_FLAG_R;
            break;
        case 'w':
        case 'W':
            *pflags |= HNDL_FLAG_W;
            break;
        case 'c':
            *pmode = HNDL_MODE_CREATE;
            break;
        case 'i':
        case 'I':
            printf("DO info\n");
            *do_info = true;
            break;
        default:
            break;
        }
    }
}

static int __do_volume_info(VOLUME_HNDL volume)
{
    lightos_volume_t lvolume;

    if (volume_get(volume, &lvolume))
        return -EINVAL;

    printf("Got volume: %s (ID: %d, Parent ID: %d)\n", lvolume.volume_label, lvolume.id, lvolume.device_id);
    printf(" - Logical sector size: %d\n", lvolume.logical_sector_size);
    printf(" - Physical sector size: %d\n", lvolume.physical_sector_size);
    printf(" - Maximum addressable offset: %lld\n", lvolume.max_offset);

    lightos_volume_clean(&lvolume);
    return 0;
}

int main()
{
    bool do_info;
    uint32_t vflags;
    enum HNDL_MODE mode;
    CMDLINE cmdline = { 0 };

    /* Grab the commandline */
    if (cmdline_get(&cmdline))
        return -ENOCMD;

    printf("Target: %s\n", target_volume);

    /* Parse the mode string */
    __parse_target_mode(&vflags, &mode, &do_info);

    /* Open the actual volume */
    volume = open_volume(target_volume, vflags, mode);

    /* Fuck, could not find the device */
    if (!handle_verify(volume))
        return -ENODEV;

    printf("[DEBUG] volume=%s, mode=%s, offset=%lld, size=%lld\n", target_volume, target_mode, target_offset, target_size);

    return __do_volume_info(volume);

    return 0;
}

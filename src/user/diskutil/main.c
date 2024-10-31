#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include <errno.h>
#include <lightos/proc/cmdline.h>
#include <params/params.h>
#include <stdio.h>
#include <volumeio/volumeio.h>

/* Parameters */
static const char target_volume[256] = { 0 };
static const char target_mode[8] = { 0 };
static uint64_t target_offset = 0;
static size_t target_size = 0;

/* Static program data */
static VOLUME_HNDL volume;

/* List for the params library */
static cmd_param_t params[] = {
    CMD_PARAM_1_ALIAS(target_volume, sizeof(target_volume), CMD_PARAM_TYPE_STRING, 'd', "Specify the target device", "--device"),
    CMD_PARAM_1_ALIAS(target_mode, sizeof(target_mode), CMD_PARAM_TYPE_STRING, 'm', "Select the mode of opperation (\'i\'=info, \'r\'=read)", "--mode"),
    CMD_PARAM_1_ALIAS(target_offset, sizeof(target_offset), CMD_PARAM_TYPE_NUM, 'o', "Specify the offset into the volume", "--offset"),
    CMD_PARAM_1_ALIAS(target_size, sizeof(target_size), CMD_PARAM_TYPE_NUM, 's', "Specify the size of opperation into the volume", "--size"),
};

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
    printf(" - Maximum addressable block: %lld\n", lvolume.max_blk);

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

    /* Parse the commandline into the params struct */
    if (params_parse((const char**)cmdline.argv, cmdline.argc, params, (sizeof params / sizeof params[0])))
        return -EINVAL;

    /* Parse the mode string */
    __parse_target_mode(&vflags, &mode, &do_info);

    /* Open the actual volume */
    volume = open_volume(target_volume, vflags, mode);

    /* Fuck, could not find the device */
    if (!handle_verify(volume))
        return -ENODEV;

    printf("[DEBUG] volume=%s, mode=%s, offset=0x%llx, size=0x%llx\n", target_volume, target_mode, target_offset, target_size);

    if (do_info)
        return __do_volume_info(volume);

    return 0;
}

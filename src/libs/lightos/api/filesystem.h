#ifndef __LIGHTOS_FS_SHARED__
#define __LIGHTOS_FS_SHARED__

#include "lightos/api/objects.h"

enum LIGHTOS_FSTYPE {
    LIGHTOS_FSTYPE_FAT32,
    LIGHTOS_FSTYPE_FAT16,
    LIGHTOS_FSTYPE_FAT12,
    LIGHTOS_FSTYPE_EXT2,
    LIGHTOS_FSTYPE_EXT3,
    LIGHTOS_FSTYPE_EXT4,
    LIGHTOS_FSTYPE_CRAMFS,
};

/* TODO */

typedef struct lightos_file {
    Object object;

    /* User-side caches */
    void* wr_buff;
    void* rd_buff;
    size_t wr_bsize;
    size_t rd_bsize;

} File;

#endif // ! __LIGHTOS_FS_SHARED__

#ifndef __LIGHTOS_FS_SHARED__
#define __LIGHTOS_FS_SHARED__

#include "lightos/api/objects.h"

/* Default buffer sizes */
#define LIGHTOS_FILE_DEFAULT_RBSIZE 0x1000
#define LIGHTOS_FILE_DEFAULT_WBSIZE 0x1000

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

/*
 * LightOS File structure
 *
 * Represents a generic byte bucket that may or may not be connected to OSS.
 * It can be used to store named buffers of data, or store stuff to long term
 * storage (on oss)
 */
typedef struct lightos_file {
    Object object;

    /* User-side caches */
    u8* wr_buff;
    u8* rd_buff;
    /* Size of the read and write bufferse */
    size_t wr_bsize;
    size_t rd_bsize;

    /* Read/Write offsets */
    u64 wr_bstart;
    /* Head offset where the buffer starts in the backing file */
    u64 wr_bhead;
    /* How many writes have hit the cache since the last flush */
    u32 wr_cache_hit_count;

    /* Read offset inside the local file buffer */
    u64 rd_bstart;
    /* Read/Write head */
    u64 rd_bhead;
} File;

#endif // ! __LIGHTOS_FS_SHARED__

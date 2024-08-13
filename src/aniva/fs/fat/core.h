#ifndef __ANIVA_GENERIC_FAT__
#define __ANIVA_GENERIC_FAT__
#include "dev/disk/shared.h"
#include "fs/fat/cache.h"
#include "sync/mutex.h"
#include <fs/core.h>
#include <libk/stddef.h>

struct oss_node;
struct fat_file;
struct fat_file_ops;

#define FAT_SECTOR_SIZE 512

#define FAT_LFN_LEN 255 /* maximum long name length */
#define FAT_MAX_NAME 11 /* maximum name length */
#define FAT_MAX_NAME_SLOTS 21 /* max # of slots for short and long names */
#define MSDOS_DOT ".          " /* ".", padded to MSDOS_NAME chars */
#define MSDOS_DOTDOT "..         " /* "..", padded to MSDOS_NAME chars */

#define FAT_LFN_LAST_ENTRY 0x40

/* start of data cluster's entry (number of reserved clusters) */
#define FAT_START_ENT 2

#define DELETED_FLAG 0xe5 /* marks file as deleted when in name[0] */
#define IS_FREE(n) (!*(n) || *(n) == DELETED_FLAG)

/* maximum number of clusters */
#define MAX_FAT12 0xFF4
#define MAX_FAT16 0xFFF4
#define MAX_FAT32 0x0FFFFFF6

/* bad cluster mark */
#define BAD_FAT12 0xFF7
#define BAD_FAT16 0xFFF7
#define BAD_FAT32 0x0FFFFFF7

/* standard EOF */
#define EOF_FAT12 0xFFF
#define EOF_FAT16 0xFFFF
#define EOF_FAT32 0x0FFFFFeF

#define FAT_ENT_FREE (0)
#define FAT_ENT_BAD (BAD_FAT32)
#define FAT_ENT_EOF (EOF_FAT32)

#define FAT_ATTR_RO 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LFN (FAT_ATTR_RO | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)
#define FAT_ATTR_LFN_MASK (FAT_ATTR_LFN | FAT_ATTR_DIR | FAT_ATTR_ARCHIVE)

#define FAT_STATE_DIRTY 0x01

/*
 * The fat boot sector
 */
typedef struct fat_boot_sector {
    uint8_t unused[3]; /* Bootstrap funnie */
    uint8_t sys_id[8]; /* Name */

    uint16_t sector_size;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats;
    uint16_t root_dir_entries;
    uint16_t sector_count_fat16;
    uint8_t media;
    uint16_t sectors_per_fat16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden;
    uint32_t sector_count_fat32;

    union {
        struct {
            uint8_t drive_num;
            uint8_t state;
            uint8_t signature;
            uint8_t volume_id[4];
            uint8_t volume_label[FAT_MAX_NAME];
            uint8_t fs_type[8];
        } fat16;

        struct {
            uint32_t sectors_per_fat;
            uint16_t flags;
            uint8_t version[2];
            uint32_t root_cluster;
            uint16_t info_sector;
            uint16_t backup_boot;
            uint8_t reserved[12];

            uint8_t drive_num;
            uint8_t state;
            uint8_t signature;
            uint8_t volume_id[4];
            uint8_t volume_label[FAT_MAX_NAME];
            uint8_t fs_type[8];
        } fat32;
    };
} __attribute__((packed)) fat_boot_sector_t;

static inline bool fat_valid_media(uint8_t media)
{
    return 0xf8 <= media || media == 0xf0;
}

static inline bool fat_valid_bs(fat_boot_sector_t* bs)
{
    return fat_valid_media(bs->media);
}

/*
 * Lil bit of data that is most commonly found on sector 2
 */
typedef struct fat_boot_fsinfo {
    uint32_t signature0;
    uint32_t res0[120];
    uint32_t signature1;
    uint32_t free_clusters;
    uint32_t most_recent_clutser;
    uint32_t res1[4];
} __attribute__((packed)) fat_boot_fsinfo_t;

/*
 * A generic directory entry for this filesystem
 */
typedef struct fat_dir_entry {
    uint8_t name[FAT_MAX_NAME];
    uint8_t attr;
    uint8_t lcase;
    uint8_t ctime_cs;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t first_cluster_hi;
    uint16_t time;
    uint16_t date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

/* TODO: */
typedef struct {
    disk_offset_t pos;
    disk_offset_t offset;

    fat_dir_entry_t* dir_entry;
} fat_dir_t;

typedef struct {
    u8 idx_ord;
    u16 name_0[5];
    u8 attr;
    u8 type;
    u8 chksum;
    u16 name_1[6];
    u16 fst_clus_lo;
    u16 name_2[2];
} __attribute__((packed)) fat_lfn_entry_t;

#define FTYPE_FAT32 (32)
#define FTYPE_FAT16 (16)
#define FTYPE_FAT12 (12)
#define FTYPE_FATex (64) /* ? */

/*
 * Specific data regarding the FAT filesystem we encounter
 * TODO: should this hold caches?
 */
typedef struct fat_fs_info {
    struct oss_node* node;

    uint8_t fats, fat_type; /* FAT count and bits of this FAT fs (12, 16, 32) */
    uint8_t fat_file_shift, has_dirty;
    uint32_t first_valid_sector;

    uint32_t fat_start; // The File Allocation Table start sector
    uint32_t fat_len; // The length of the FAT in sectors

    mutex_t* fat_lock;

    struct fat_file_ops* m_file_ops;

    uint32_t root_directory_sectors;
    uint32_t total_reserved_sectors;
    uint32_t total_usable_sectors;
    uint32_t cluster_count;
    uint32_t cluster_size;
    uintptr_t usable_clusters_start;
    uint32_t usable_sector_offset;

    /* at-mount copy of the root entry */
    fat_dir_entry_t root_entry_cpy;

    fat_boot_sector_t boot_sector_copy;
    fat_boot_fsinfo_t boot_fs_info;

    fat_sector_cache_t* sector_cache;

} fat_fs_info_t;

#define GET_FAT_FSINFO(node) ((fat_fs_info_t*)(oss_node_getfs((node))->m_fs_priv))

static inline bool is_fat32(fat_fs_info_t* finfo)
{
    return (finfo->fat_type == FTYPE_FAT32);
}

static inline bool is_fat16(fat_fs_info_t* finfo)
{
    return (finfo->fat_type == FTYPE_FAT16);
}

static inline bool is_fat12(fat_fs_info_t* finfo)
{
    return (finfo->fat_type == FTYPE_FAT12);
}

int fat32_read_clusters(oss_node_t* node, uint8_t* buffer, struct fat_file* file, uint32_t start, size_t size);
int fat32_write_clusters(oss_node_t* node, uint8_t* buffer, struct fat_file* ffile, uint32_t offset, size_t size);
int fat32_read_dir_entry(oss_node_t* node, struct fat_file* dir, fat_dir_entry_t* out, uint32_t idx, uint32_t* diroffset);

#endif // !__ANIVA_GENERIC_FAT__

#ifndef __ANIVA_GENERIC_FAT__
#define __ANIVA_GENERIC_FAT__
#include "dev/disk/shared.h"
#include "fs/superblock.h"
#include "fs/vnode.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

#define FAT_SECTOR_SIZE 512

#define FAT_LFN_LEN	255	/* maximum long name length */
#define FAT_MAX_NAME	11	/* maximum name length */
#define FAT_MAX_SLOTS	21	/* max # of slots for short and long names */
#define MSDOS_DOT	".          "	/* ".", padded to MSDOS_NAME chars */
#define MSDOS_DOTDOT	"..         "	/* "..", padded to MSDOS_NAME chars */

/* start of data cluster's entry (number of reserved clusters) */
#define FAT_START_ENT	2

#define DELETED_FLAG	0xe5	/* marks file as deleted when in name[0] */
#define IS_FREE(n)	(!*(n) || *(n) == DELETED_FLAG)

/* maximum number of clusters */
#define MAX_FAT12	0xFF4
#define MAX_FAT16	0xFFF4
#define MAX_FAT32	0x0FFFFFF6

/* bad cluster mark */
#define BAD_FAT12	0xFF7
#define BAD_FAT16	0xFFF7
#define BAD_FAT32	0x0FFFFFF7

/* standard EOF */
#define EOF_FAT12	0xFFF
#define EOF_FAT16	0xFFFF
#define EOF_FAT32	0x0FFFFFFF

#define FAT_ENT_FREE	(0)
#define FAT_ENT_BAD	(BAD_FAT32)
#define FAT_ENT_EOF	(EOF_FAT32)

#define FAT_ATTR_RO 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LFN (FAT_ATTR_RO | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

/*
 * The fat boot sector
 */
typedef struct {
  uint8_t unused[3]; /* Bootstrap funnie */
  uint8_t sys_id[8]; /* Name */

  uint16_t sector_size;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fats;
  uint16_t dir_entries;
  uint16_t sectors;
  uint8_t media;
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t heads;
  uint32_t hidden;
  uint32_t total_sectors;

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
      uint16_t reserved[2];

      uint8_t drive_num;
      uint8_t state;
      uint8_t signature;
      uint8_t volume_id[4];
      uint8_t volume_label[FAT_MAX_NAME];
      uint8_t fs_type[8];
    } fat32;
  };
} __attribute__((packed)) fat_boot_sector_t;

static inline bool fat_valid_media(uint8_t media) {
  return 0xf8 <= media || media == 0xf0;
}

static inline bool fat_valid_bs(fat_boot_sector_t* bs){
	return fat_valid_media(bs->media);
}

typedef struct {
  uint32_t signature0;
  uint32_t res0[120];
  uint32_t signature1;
  uint32_t free_clusters;
  uint32_t most_recent_clutser;
  uint32_t res1[4];
} __attribute__((packed)) fat_boot_fsinfo_t;

typedef struct {
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
  uint16_t first_clutser_low;
  uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

/* TODO: */
typedef struct {
  disk_offset_t pos;
  disk_offset_t offset;

  fat_dir_entry_t* dir_entry;
} fat_dir_t;

typedef struct {

} __attribute__((packed)) fat_lfn_entry_t;

/*
 * Specific data regarding the FAT filesystem we encounter
 * TODO: should this hold caches?
 */
typedef struct {
  uint16_t sectors_per_cluster;
  uint32_t cluster_size;
  uint16_t first_fat;

  uint8_t fat_type; /* bits of this FAT fs (12, 16, 32) */

  

  mutex_t* fat_lock;

} fat_fs_info_t;

static fat_fs_info_t* create_fat_fs_info() {
  fat_fs_info_t* ret = kmalloc(sizeof(fat_fs_info_t));
  memset(ret, 0, sizeof(fat_fs_info_t));

  ret->fat_lock = create_mutex(0);

  return ret;
}

void destroy_fat_fs_info(fat_fs_info_t* info) {
  destroy_mutex(info->fat_lock);
  kfree(info);
}

size_t fat_calculate_clusters(fs_superblock_t* block);
void fat_get_cluster(vnode_t* node, uintptr_t clustern);
ErrorOrPtr fat_get_dir_entry(vnode_t* node, uintptr_t* offset, fat_dir_entry_t** dir_entry);

ErrorOrPtr fat_find(vnode_t* node, const char* path);

/* TODO: implement FAT functions */

#endif // !__ANIVA_GENERIC_FAT__

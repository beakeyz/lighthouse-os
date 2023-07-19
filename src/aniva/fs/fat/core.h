#ifndef __ANIVA_GENERIC_FAT__
#define __ANIVA_GENERIC_FAT__
#include "dev/disk/shared.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct vnode;

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
  /* TODO */
} __attribute__((packed)) fat_lfn_entry_t;

#define FTYPE_FAT32         (32)
#define FTYPE_FAT16         (16)
#define FTYPE_FAT12         (12)
#define FTYPE_FATex         (64) /* ? */

/*
 * Specific data regarding the FAT filesystem we encounter
 * TODO: should this hold caches?
 */
typedef struct fat_fs_info {

  uint8_t fats, fat_type; /* FAT count and bits of this FAT fs (12, 16, 32) */
  uint8_t fat_file_shift, has_dirty;
  uint32_t first_valid_sector;

  uint32_t fat_start;   // The File Allocation Table start sector
  uint32_t fat_len;     // The length of the FAT in sectors

  uint32_t root_dir_cluster;
  uint32_t max_cluster;

  uint32_t dir_start_cluster;
  uint32_t dir_entries;

  size_t total_fs_size;

  mutex_t* fat_lock;

  fat_boot_sector_t boot_sector_copy;
  fat_boot_fsinfo_t boot_fs_info;

} fat_fs_info_t;

#define FAT_FSINFO(node) ((fat_fs_info_t*)((node)->fs_data.m_fs_specific_info))

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

typedef struct fat_file {

  int entry;
  void* clusterchain_buffer;
  size_t clusterchain_size;

  /* Index into the clusterchain */
  union {
    uint8_t* index_ft12[2]; // mostly unused
    uint16_t* index_ft16; // mostly unused
    uint32_t* index_ft32;
  } idx;

  struct vnode* parent_node;
} fat_file_t;

/*
 * Clear without doing any buffer work
 */
static void clear_fat_file(fat_file_t* file) 
{
  file->parent_node = NULL;
  file->entry = 0;
  file->idx.index_ft32 = 0;
  file->clusterchain_size = 0;
  file->clusterchain_buffer = nullptr;
}

static void fat_file_set_entry(fat_file_t* file, int entry)
{
  file->entry = entry;
  file->idx.index_ft32 = NULL;
}

extern int fat_prepare_finfo(struct vnode* node);

extern int ffile_read(fat_file_t* f, int e);
extern int ffile_write(fat_file_t* f, int e);

#endif // !__ANIVA_GENERIC_FAT__

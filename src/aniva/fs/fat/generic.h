#ifndef __ANIVA_GENERIC_FAT__
#define __ANIVA_GENERIC_FAT__
#include <libk/stddef.h>

#define FAT_SECTOR_SIZE 512

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
  uint8_t jmp_boot[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t fat_num;
  uint16_t root_entries_count;
  uint16_t sector_num_fat16;
  uint8_t media_type;
  uint16_t sectors_per_fat16;
  uint16_t sectors_per_track;
  uint16_t heads_num;
  uint32_t hidden_sector_num;
  uint32_t sector_num_fat32;
  uint32_t sectors_num_per_fat;
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info_sector;
  uint16_t mbr_copy_sector;
  uint8_t reserved[12];
  uint8_t drive_number;

  uint8_t reserved1;
  uint8_t signature;
  uint32_t volume_id;
  char volume_label[11];
  char system_type[8];
} __attribute__((packed)) fat_bpb_t;

typedef struct {

} __attribute__((packed)) fat_dir_entry_t;

typedef struct {

} __attribute__((packed)) fat_lfn_entry_t;

#endif // !__ANIVA_GENERIC_FAT__

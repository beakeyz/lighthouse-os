#ifndef __ANIVA_GENERIC_DISK_DEV__
#define __ANIVA_GENERIC_DISK_DEV__
#include "dev/debug/serial.h"
#include "dev/disk/partition/mbr.h"
#include "dev/disk/shared.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include <libk/stddef.h>

// TODO: generic error codes

typedef uint8_t disk_uid_t;

struct disk_dev;
struct partitioned_disk_dev;

struct gpt_table;
struct mbr_table;

typedef struct generic_disk_ops {
  int (*f_read) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_read_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
} generic_disk_ops_t;

#define PART_TYPE_NONE              (0)
#define PART_TYPE_GPT               (1)
#define PART_TYPE_MBR               (2)

typedef struct disk_dev {
  void* m_parent;

  const char* m_device_name;
  char* m_path;

  uint32_t m_flags;

  uint16_t m_reserved;

  /* Generic uid that acts as an index into the list of device that are available */
  disk_uid_t m_uid;         // 8 bits
  uint8_t m_partition_type; // 8 bits

  uint16_t m_firmware_rev[4]; // 64 bits

  uintptr_t m_max_blk;
  size_t m_logical_sector_size;
  size_t m_physical_sector_size;

  generic_disk_ops_t m_ops;

  size_t m_partitioned_dev_count;
  struct partitioned_disk_dev* m_devs;

  /* We'll cache the partition structure here, for if we want to make modifications */
  union {
    struct gpt_table* gpt_table;
    struct mbr_table* mbr_table;
  };
} disk_dev_t;

#define GDISKDEV_FLAG_SCSI                   (0x00000001) /* Does this device use SCSI */
#define GDISKDEV_FLAG_RAM                    (0x00000002) /* Is this a ramdevice */
#define GDISKDEV_FLAG_RAM_COMPRESSED         (0x00000004) /* Is this a compressed ramdevice */
#define GDISKDEV_FLAG_RO                     (0x00000008)
#define GDISKDEV_FLAG_NO_PART                (0x00000010)
#define GDISKDEV_FLAG_LEGACY                 (0x00000020)

#define PD_FLAG_ONLY_SYNC                    (0x00000001) /* This partitioned device can't use async IO */

typedef struct partitioned_disk_dev {
  struct disk_dev* m_parent;

  char* m_name;
  uint64_t m_start_lba;
  uint64_t m_end_lba;

  uint32_t m_flags;
  uint32_t m_block_size;

  struct partitioned_disk_dev* m_next;
} partitioned_disk_dev_t;

void destroy_generic_disk(disk_dev_t* device);

int read_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset);
int write_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset);
int read_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block);
int write_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block);

/* Read or Write to a partitioned device using a caller-controlled buffer */
int pd_bread(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn);
int pd_bwrite(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn);

int pd_set_blocksize(partitioned_disk_dev_t* dev, uint32_t blksize);
/*
 * Find the disk device from which we where booted in
 * order to read config files and load ramdisks
 */
void register_boot_device();

void init_gdisk_dev();
void init_root_device_probing();

ErrorOrPtr register_gdisk_dev(disk_dev_t* device);
ErrorOrPtr register_gdisk_dev_with_uid(disk_dev_t* device, disk_uid_t uid);
ErrorOrPtr unregister_gdisk_dev(disk_dev_t* device);

bool gdisk_is_valid(disk_dev_t* device);

int diskdev_populate_partition_table(disk_dev_t* dev);

/*
 * find the partitioned device (weird naming sceme but its whatever) 
 * of a generic disk device at idx
 * idx is a dword, since the number of partitions should never exceed that limit lmao
 */
partitioned_disk_dev_t* find_partition_at(disk_dev_t* diskdev, uint32_t idx);

disk_dev_t* find_gdisk_device(disk_uid_t uid);

partitioned_disk_dev_t* create_partitioned_disk_dev(disk_dev_t* parent, char* name, uint64_t start, uint64_t end, uint32_t flags);
void destroy_partitioned_disk_dev(partitioned_disk_dev_t* dev);

/*
 * TODO: attach device limit
 */
void attach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev);
void detach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev);
partitioned_disk_dev_t** fetch_designated_device_for_block(disk_dev_t* generic, uintptr_t block);
int generic_disk_opperation(disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset);

#endif // !__ANIVA_GENERIC_DISK_DEV__

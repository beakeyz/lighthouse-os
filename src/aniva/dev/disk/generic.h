#ifndef __ANIVA_GENERIC_DISK_DEV__
#define __ANIVA_GENERIC_DISK_DEV__
#include "dev/debug/serial.h"
#include "dev/disk/partition/generic.h"
#include "dev/disk/shared.h"
#include "dev/handle.h"
#include "fs/superblock.h"
#include "mem/heap.h"
#include <libk/stddef.h>

// TODO: generic error codes

typedef uint8_t disk_uid_t;

struct disk_dev;
struct partitioned_disk_dev;

typedef struct generic_disk_ops {
  int (*f_read) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_read_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
} generic_disk_ops_t;

typedef struct disk_dev {
  handle_t m_parent;

  const char* m_device_name;
  char* m_path;

  /* Generic uid that acts as an index into the list of device that are available */
  disk_uid_t m_uid;

  uint32_t m_flags;

  uint16_t m_firmware_rev[4];

  uintptr_t m_max_blk;
  size_t m_logical_sector_size;
  size_t m_physical_sector_size;

  generic_disk_ops_t m_ops;

  size_t m_partitioned_dev_count;
  struct partitioned_disk_dev* m_devs;
} generic_disk_dev_t;

#define GDISKDEV_SCSI                   (0x00000001) /* Does this device use SCSI */
#define GDISKDEV_RAM                    (0x00000002) /* Is this a ramdevice */
#define GDISKDEV_RAM_COMPRESSED         (0x00000002) /* Is this a compressed ramdevice */

typedef struct partitioned_disk_dev {
  struct disk_dev* m_parent;

  fs_superblock_t* m_superblock;

  generic_partition_t m_partition_data;

  struct partitioned_disk_dev* m_next;
} partitioned_disk_dev_t;

int read_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset);
int write_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset);
int read_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block);
int write_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block);

/*
 * Find the disk device from which we where booted in
 * order to read config files and load ramdisks
 */
void register_boot_device();

void init_gdisk_dev();
void init_root_device_probing();

ErrorOrPtr register_gdisk_dev(generic_disk_dev_t* device);
ErrorOrPtr register_gdisk_dev_with_uid(generic_disk_dev_t* device, disk_uid_t uid);
ErrorOrPtr unregister_gdisk_dev(generic_disk_dev_t* device);

generic_disk_dev_t* find_gdisk_device(disk_uid_t uid);

static partitioned_disk_dev_t* create_partitioned_disk_dev(generic_disk_dev_t* parent, generic_partition_t partition) {
  partitioned_disk_dev_t* ret = kmalloc(sizeof(partitioned_disk_dev_t));
  
  ret->m_parent = parent;

  ret->m_partition_data = partition;
  ret->m_next = nullptr;

  return ret;
}

static void destroy_partitioned_disk_dev(partitioned_disk_dev_t* dev) {
  kfree(dev);
}

static void attach_partitioned_disk_device(generic_disk_dev_t* generic, partitioned_disk_dev_t* dev) {

  if (!generic->m_devs) {
    generic->m_devs = dev;
    return;
  }

  partitioned_disk_dev_t** device;
  
  // Simply attach to the end of the linked list
  for (device = &generic->m_devs; *device; device = &(*device)->m_next) {
    if (!(*device)) {
      break;
    }
  }

  if (!(*device)) {
    *device = dev;
  }
}

static void detach_partitioned_disk_device(generic_disk_dev_t* generic, partitioned_disk_dev_t* dev) {

  if (!dev)
    return;

  partitioned_disk_dev_t** previous = nullptr;
  partitioned_disk_dev_t** device;

  for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
    if ((*device) == dev) {
      if (previous) {
        (*previous)->m_next = (*device)->m_next;
      }
      break;
    }

    previous = device;
  }
}

static partitioned_disk_dev_t** fetch_designated_device_for_block(generic_disk_dev_t* generic, uintptr_t block) {

  partitioned_disk_dev_t** device;
  
  for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
    uintptr_t start_lba = (*device)->m_partition_data.m_start_lba;
    uintptr_t end_lba = (*device)->m_partition_data.m_end_lba;

    // Check if the block is contained in this partition
    if (block >= start_lba && block <= end_lba) {
      return device;
    }
  }

  return nullptr;
}

static ALWAYS_INLINE int generic_disk_opperation(generic_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {
  return -1;
}

#endif // !__ANIVA_GENERIC_DISK_DEV__

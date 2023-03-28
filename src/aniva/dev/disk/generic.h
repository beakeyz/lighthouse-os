#ifndef __ANIVA_GENERIC_DISK_DEV__
#define __ANIVA_GENERIC_DISK_DEV__
#include "dev/disk/shared.h"
#include "dev/handle.h"
#include <libk/stddef.h>

// TODO: generic error codes

typedef struct disk_dev {
  handle_t m_parent;

  const char* m_device_name;

  uintptr_t m_max_blk;
  size_t m_logical_sector_size;
  size_t m_physical_sector_size;

  int (*f_read) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_read_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
  int (*f_write_sync) (struct disk_dev* parent, void* buffer, size_t size, disk_offset_t offset);
} generic_disk_dev_t;

static ALWAYS_INLINE int generic_disk_opperation(generic_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {
  return -1;
}

#endif // !__ANIVA_GENERIC_DISK_DEV__

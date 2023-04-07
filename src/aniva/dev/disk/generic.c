#include "generic.h"
#include "libk/error.h"

int read_sync_partitioned_block(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {

  int result = -1;
  disk_offset_t offset;

  if (block >= dev->m_partition_data.m_start_lba && block <= dev->m_partition_data.m_end_lba) {

    offset = block * dev->m_parent->m_logical_sector_size;

    if (dev->m_ops.f_read_sync) {
      result = dev->m_ops.f_read_sync(dev->m_parent, buffer, size, offset);
    } else {
      // Do we have an alternative?
    }
  }

  return result;
}

int write_sync_partitioned_block(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {
  kernel_panic("TODO: implement write_sync_partitioned_block");
}

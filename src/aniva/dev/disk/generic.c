#include "generic.h"
#include "libk/error.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "ramdisk.h"

int read_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {

  int result = -1;

  if (!dev || !dev->m_parent)
    return result;


  const uintptr_t block = offset * dev->m_parent->m_logical_sector_size;

  if (block >= dev->m_partition_data.m_start_lba && block <= dev->m_partition_data.m_end_lba) {

    if (dev->m_parent->m_ops.f_read_sync)
      result = dev->m_parent->m_ops.f_read_sync(dev->m_parent, buffer, size, offset);

  }

  return result;
}

int write_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement write_sync_partitioned");
  return 0;
}

int read_sync_partitioned_block(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {

  if (!dev || !dev->m_parent)
    return -1;

  int result = -1;
  disk_offset_t offset;

  block = dev->m_partition_data.m_start_lba + block;

  if (block <= dev->m_partition_data.m_end_lba) {

    offset = block * dev->m_parent->m_logical_sector_size;

    if (dev->m_parent->m_ops.f_read_sync) {
      result = dev->m_parent->m_ops.f_read_sync(dev->m_parent, buffer, size, offset);
    } else {
      // Do we have an alternative?
    }
  }

  return result;
}

int write_sync_partitioned_block(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {
  kernel_panic("TODO: implement write_sync_partitioned_block");
}

generic_disk_dev_t* create_generic_ramdev(void* start_addr, size_t size) {

  generic_disk_dev_t* dev = kmalloc(sizeof(generic_disk_dev_t));
  memset(dev, 0x00, sizeof(generic_disk_dev_t));

  dev->m_flags |= GDISKDEV_RAM;
  dev->m_parent = nullptr;
  dev->m_ops.f_read_sync = ramdisk_read;
  dev->m_ops.f_write_sync = ramdisk_write;
  dev->m_logical_sector_size = 512;
  dev->m_physical_sector_size = 512;
  dev->m_device_name = "ramdev"; /* Should we create a unique name? */
  dev->m_max_blk = size / 512;

  generic_partition_t part = {
    .m_start_lba = 0,
    .m_end_lba = dev->m_max_blk,
    .m_name = "rampart0",
  };

  partitioned_disk_dev_t* partdev = create_partitioned_disk_dev(dev, part);
  
  attach_partitioned_disk_device(dev, partdev);

  return dev;
}

int ramdisk_read(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {

  return 0;
}

int ramdisk_write(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement ramdisk_write");
}

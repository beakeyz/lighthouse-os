#include "generic.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"
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

generic_disk_dev_t* create_generic_ramdev(size_t size) {

  size = ALIGN_UP(size, SMALL_PAGE_SIZE);
  uintptr_t start_addr = Must(kmem_kernel_alloc_range(size, KMEM_CUSTOMFLAG_CREATE_USER, 0));

  generic_disk_dev_t* dev = create_generic_ramdev_at(start_addr, size);

  return dev;
}

/*
 * We leave it to the called to map the addressspace
 * TODO: maybe we could pass an addressspace object that
 * enforces mappings...
 */
generic_disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size) {
  /* Trolle xD */
  const size_t ramdisk_blksize = 1;
  size = ALIGN_UP(size, SMALL_PAGE_SIZE);
  paddr_t start_addr = address;

  memset((void*)start_addr, 0x00, size);

  generic_disk_dev_t* dev = kmalloc(sizeof(generic_disk_dev_t));
  memset(dev, 0x00, sizeof(generic_disk_dev_t));

  dev->m_flags |= GDISKDEV_RAM;
  dev->m_parent = nullptr;
  dev->m_ops.f_read_sync = ramdisk_read;
  dev->m_ops.f_write_sync = ramdisk_write;
  dev->m_logical_sector_size = ramdisk_blksize;
  dev->m_physical_sector_size = ramdisk_blksize;
  dev->m_device_name = "ramdev"; /* Should we create a unique name? */
  dev->m_max_blk = start_addr + size;

  generic_partition_t part = {
    .m_start_lba = start_addr,
    .m_end_lba = dev->m_max_blk,
    .m_name = "rampart0",
  };

  partitioned_disk_dev_t* partdev = create_partitioned_disk_dev(dev, part);
  
  attach_partitioned_disk_device(dev, partdev);

  return dev;

}

bool destroy_generic_ramdev(generic_disk_dev_t* device) {

  /* If we get passed a faulty ramdevice, just die */
  if (!device || (device->m_flags & GDISKDEV_RAM) == 0 || !device->m_devs || device->m_devs->m_next)
    return false;

  const uintptr_t start_addr = device->m_devs->m_partition_data.m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_partition_data.m_end_lba;

  const size_t ramdev_size = end_addr - start_addr;

  ErrorOrPtr result = kmem_kernel_dealloc(start_addr, ramdev_size);

  if (result.m_status != ANIVA_SUCCESS)
    return false;

  /* We should only have one partition, so this works */
  destroy_partitioned_disk_dev(device->m_devs);

  kfree(device);

  return true;
}

int ramdisk_read(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {

  if (!device || !device->m_devs || !device->m_physical_sector_size)
    return -1;

  if (!buffer || !size)
    return -1;

  /* A ramdisk device always only contains one 'partition' */
  if (device->m_devs->m_next)
    return -1;

  const uintptr_t start_addr = device->m_devs->m_partition_data.m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_partition_data.m_end_lba;

  const uintptr_t read_addr = start_addr + offset;

  if (read_addr > end_addr || read_addr + size > end_addr)
    return -1;

  memcpy(buffer, (void*)read_addr, size);

  return 0;
}

int ramdisk_write(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement ramdisk_write");
}

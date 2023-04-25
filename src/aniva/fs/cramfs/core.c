#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/cramfs/compression.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "mem/kmem_manager.h"
#include <fs/core.h>

int ramfs_init();
int ramfs_exit();

static int ramfs_read(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {

  kernel_panic("TODO: implement ramfs_read");

  return 0;
}

static int ramfs_write(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {
  kernel_panic("TODO: implement ramfs_write");
  return 0;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
vnode_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  println("Mounting ramfs");

  /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
  const generic_disk_dev_t* parent = device->m_parent;

  ASSERT_MSG(parent->m_devs == device, "Ramdisk partition mismatch!");

  if (!parent)
    return nullptr;

  if ((parent->m_flags & GDISKDEV_RAM) == 0)
    return nullptr;

  const size_t partition_size = ALIGN_UP(device->m_partition_data.m_end_lba - device->m_partition_data.m_start_lba, SMALL_PAGE_SIZE);
  vnode_t* node = create_generic_vnode("cramfs", VN_MOUNT | VN_FS);

  node->m_size = partition_size;
  node->m_data = (void*)Must(kmem_kernel_alloc_range(partition_size, 0, 0));

  if (parent->m_flags & GDISKDEV_RAM_COMPRESSED) {

    uint32_t* uncompressed_data = (void*)device->m_partition_data.m_start_lba;

    /* Is enforcing success here a good idea? */
    Must(cram_decompress(device, node->m_data));
    kernel_panic("Yeet");

    ASSERT_MSG(node->m_data != nullptr, "decompressing resulted in NULL");

    /* Free the pages of the compressed ramdisk */
    kmem_kernel_dealloc(device->m_partition_data.m_start_lba, kmem_get_page_idx(node->m_size + SMALL_PAGE_SIZE - 1));

    device->m_partition_data.m_start_lba = (uintptr_t)node->m_data;
    device->m_partition_data.m_end_lba = (uintptr_t)node->m_data + partition_size;
  }

  node->f_read = ramfs_read;
  node->f_write = ramfs_write;

  return node;
}

aniva_driver_t ramfs = {
  .m_name = "cramfs",
  .m_type = DT_FS,
  .f_init = ramfs_init,
  .f_exit = ramfs_exit,
};
EXPORT_DRIVER(ramfs);

fs_type_t cramfs = {
  .m_name = "cramfs",
  .f_mount = mount_ramfs,
  .m_driver = &ramfs,
  .m_flags = FST_REQ_DRIVER
};

int ramfs_init() {
  println("Initialized ramfs");

  int ret = 0;
  ErrorOrPtr result = register_filesystem(&cramfs);

  if (result.m_status == ANIVA_FAIL) {
    kernel_panic("Failed to register filesystem");
    ret = -1;
  }

  return ret;
}

int ramfs_exit() {

  return 0;
}

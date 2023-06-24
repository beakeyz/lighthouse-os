#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/cramfs/compression.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <fs/core.h>

#include <dev/kterm/kterm.h>

int ramfs_init();
int ramfs_exit();

typedef struct tar_file {
  uint8_t fn[100];
  uint8_t mode[8];
  uint8_t owner_id[8];
  uint8_t gid[8];

  uint8_t size[12];
  uint8_t m_time[12];

  uint8_t checksum[8];

  uint8_t type;
  uint8_t lnk[100];

  uint8_t ustar[6];
  uint8_t version[2];

  uint8_t owner[32];
  uint8_t group[32];

  uint8_t d_maj[8];
  uint8_t d_min[8];

  uint8_t prefix[155];
} tar_file_t;

typedef enum TAR_TYPE {
  TAR_TYPE_FILE = '0',
  TAR_TYPE_HARD_LINK = '1',
  TAR_TYPE_SYMLINK = '2',
  TAR_TYPE_CHAR_DEV = '3',
  TAR_TYPE_BLK_DEV = '4',
  TAR_TYPE_DIR = '5',
  TAR_TYPE_FIFO = '6',
} TAR_TYPE_t;

#define TAR_USTAR_ALIGNMENT 512

static uintptr_t decode_tar_ascii(uint8_t* str, size_t length) {
  
  uintptr_t ret = 0;
  uint8_t* c = str;

  /* Itterate to null byte */
  for (uintptr_t i = 0; i < length; i++) {
    ret *= 8;
    ret += *c - '0';
    c++;
  }

  return ret;
}

static uintptr_t apply_tar_alignment(uintptr_t val) {
  return (((val + TAR_USTAR_ALIGNMENT - 1) / TAR_USTAR_ALIGNMENT) + 1) * TAR_USTAR_ALIGNMENT;
}

static int ramfs_read(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {

  if (!node || !buffer || !size)
    return -1;

  uintptr_t start_offset = (uintptr_t)node->m_data;

  memcpy(buffer, (void*)(start_offset + offset), size);

  return 0;
}

static int ramfs_write(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {
  kernel_panic("TODO: implement ramfs_write (should we though?)");
  return 0;
}

static vobj_t* ramfs_find(vnode_t* node, char* name) {

  tar_file_t current_file = { 0 };
  uintptr_t current_offset = 0;
  size_t name_len = strlen(name);

  /* Create file early to catch vnode grabbing issues early */
  file_t* file = create_file(node, 0, name);

  if (!file)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  while (true) {
    int result = node->f_read(node, &current_file, sizeof(tar_file_t), current_offset);

    if (result)
      break;

    /* Invalid TAR archive file */
    if (!memcmp(current_file.ustar, "ustar", 5)) {
      break;
    }

    uintptr_t current_file_offset = (uintptr_t)node->m_data + current_offset;
    uintptr_t filesize = decode_tar_ascii(current_file.size, 11);

    if (memcmp(current_file.fn, name, name_len)) {
      /*
       * TODO: how do we want this to work? 
       * TODO: Create vobj 
       */

      if (current_file.type == TAR_TYPE_FILE) {
        uint8_t* data = (uint8_t*)(current_file_offset + TAR_USTAR_ALIGNMENT);

        file->m_data = data;
        file->m_size = filesize;
        file->m_offset = current_file_offset;
        file->m_obj->m_inum = current_file_offset;

        mutex_unlock(node->m_vobj_lock);
        return file->m_obj;
      }
      kernel_panic("cramfs: unsupported fsentry type");
    }

    current_offset += apply_tar_alignment(filesize);
  }

  mutex_unlock(node->m_vobj_lock);
  destroy_vobj(file->m_obj);
  kernel_panic("Did not find ramfs object =(");
  return nullptr;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
vnode_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  println_kterm("Mounting ramfs");

  /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
  const generic_disk_dev_t* parent = device->m_parent;

  ASSERT_MSG(parent->m_partitioned_dev_count == 1, "Ramdisk device should have only one partitioned device!");
  ASSERT_MSG(parent->m_devs == device, "Ramdisk partition mismatch!");

  if (!parent)
    return nullptr;

  if ((parent->m_flags & GDISKDEV_RAM) == 0)
    return nullptr;

  const size_t partition_size = ALIGN_UP(device->m_end_lba - device->m_start_lba, SMALL_PAGE_SIZE);
  vnode_t* node = create_generic_vnode(mountpoint, VN_MOUNT | VN_FS);

  node->m_dev = device;
  node->m_size = partition_size;
  node->m_data = (uint8_t*)device->m_start_lba;

  if (parent->m_flags & GDISKDEV_RAM_COMPRESSED) {
    size_t decompressed_size = cram_find_decompressed_size(device);

    ASSERT_MSG(decompressed_size, "Got a decompressed_size of zero!");

    /* We need to allocate for the decompressed size */
    node->m_data = (void*)Must(__kmem_kernel_alloc_range(decompressed_size, KMEM_CUSTOMFLAG_GET_MAKE, 0));
    node->m_size = decompressed_size;

    /* Is enforcing success here a good idea? */
    Must(cram_decompress(device, node->m_data));

    ASSERT_MSG(node->m_data != nullptr, "decompressing resulted in NULL");

    /* Free the pages of the compressed ramdisk */
    Must(__kmem_kernel_dealloc(device->m_start_lba, kmem_get_page_idx(node->m_size + SMALL_PAGE_SIZE - 1)));

    device->m_start_lba = (uintptr_t)node->m_data;
    device->m_end_lba = (uintptr_t)node->m_data + partition_size;
  }

  node->f_read = ramfs_read;
  node->f_write = ramfs_write;
  node->f_find = ramfs_find;

  /* cramfs is a flexible fs */
  node->m_flags |= VN_FLEXIBLE;

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

  /*
   * TODO: unregister
   */

  return 0;
}

#include "core.h"
#include "dev/core.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/cramfs/compression.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <fs/core.h>

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
} __attribute__((packed)) tar_file_t;

typedef uint8_t* tar_block_start_t;

typedef enum TAR_TYPE {
  TAR_TYPE_FILE = '0',
  TAR_TYPE_HARD_LINK = '1',
  TAR_TYPE_SYMLINK = '2',
  TAR_TYPE_CHAR_DEV = '3',
  TAR_TYPE_BLK_DEV = '4',
  TAR_TYPE_DIR = '5',
  TAR_TYPE_FIFO = '6',
} TAR_TYPE_t;

#define TAR_SUPERBLOCK(node) (node)->fs_data

#define TAR_BLOCK_START(node) (((node)->fs_data.m_fs_specific_info))

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

  uintptr_t start_offset = (uintptr_t)TAR_BLOCK_START(node);

  memcpy(buffer, (void*)(start_offset + offset), size);

  return 0;
}

static int tar_file_read(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
  size_t target_size;

  if (!file || !buffer || !size)
    return -1;

  if (!(*size))
    return -2;

  target_size = *size;
  *size = 0;

  if (offset >= file->m_total_size)
    return -3;

  /* Make sure we never read outside of the file */
  if (offset + target_size > file->m_total_size)
    target_size -= (offset + target_size) - file->m_total_size;

  /* ->m_buffer points to the actual data inside ramfs, so just copy it out of the ro region */
  memcpy(buffer, file->m_buffer, target_size);

  /* Adjust the read size */
  *size = target_size;

  return 0;
}

file_ops_t tar_file_ops = {
  .f_read = tar_file_read,
  0,
};

static vobj_t* ramfs_find(vnode_t* node, char* name) {

  tar_file_t current_file = { 0 };
  uintptr_t current_offset = 0;
  size_t name_len = strlen(name);

  if (!name || !name_len)
    return nullptr;

  /* Create file early to catch vnode grabbing issues early */
  file_t* file = create_file(node, FILE_READONLY, name);

  if (!file)
    return nullptr;

  mutex_lock(node->m_vobj_lock);

  while (current_offset <= node->fs_data.m_total_blocks) {
    /* Raw read :clown: */
    int result = ramfs_read(node, &current_file, sizeof(tar_file_t), current_offset);

    if (result)
      break;

    /* Invalid TAR archive file */
    if (!memcmp(current_file.ustar, "ustar", 5)) {
      break;
    }

    uintptr_t current_file_offset = (uintptr_t)TAR_BLOCK_START(node) + current_offset;
    uintptr_t filesize = decode_tar_ascii(current_file.size, 11);

    if (memcmp(current_file.fn, name, name_len)) {
      /*
       * TODO: how do we want this to work? 
       * TODO: Create vobj 
       */
      switch (current_file.type) {
        case TAR_TYPE_FILE:
          {
            uint8_t* data = (uint8_t*)(current_file_offset + TAR_USTAR_ALIGNMENT);

            /* We can just fill the files data, since it is readonly */
            file->m_buffer = data;
            file->m_buffer_size = filesize;
            file->m_total_size = filesize;
            file->m_buffer_offset = current_file_offset;
            file->m_obj->m_inum = current_file_offset;

            /* Make sure file opperations go through ramfs */
            file_set_ops(file, &tar_file_ops);

            mutex_unlock(node->m_vobj_lock);
            return file->m_obj;
          }
        case TAR_TYPE_DIR:
          {
            break;
          }
        default:
          kernel_panic("cramfs: unsupported fsentry type");
      }
    }

    current_offset += apply_tar_alignment(filesize);
  }

  mutex_unlock(node->m_vobj_lock);
  destroy_vobj(file->m_obj);
  return nullptr;
}

int ramfs_close(vnode_t* node, vobj_t* obj)
{
  return 0;
}

int ramfs_msg(vnode_t* node, driver_control_code_t code, void* buffer, size_t size)
{
  return 0;
}

static struct generic_vnode_ops ramfs_vnode_ops = {
  .f_open = ramfs_find,
  .f_close = ramfs_close,
  .f_msg = ramfs_msg,
};

static void __tar_create_superblock(vnode_t* node, partitioned_disk_dev_t* device)
{

  if (!node)
    return;

  TAR_SUPERBLOCK(node).m_first_usable_block = device->m_start_lba;
  TAR_SUPERBLOCK(node).m_blocksize = 1;
  /* free_blocks holds the aligned compressed size */
  TAR_SUPERBLOCK(node).m_free_blocks = ALIGN_UP(device->m_end_lba - device->m_start_lba, SMALL_PAGE_SIZE);
  TAR_SUPERBLOCK(node).m_total_blocks = TAR_SUPERBLOCK(node).m_free_blocks;
  /* TODO */
  TAR_SUPERBLOCK(node).m_dirty_blocks = 0;
  TAR_SUPERBLOCK(node).m_max_filesize = -1;
  TAR_SUPERBLOCK(node).m_faulty_blocks = 0;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
vnode_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  logln("Mounting ramfs");

  /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
  const disk_dev_t* parent = device->m_parent;

  ASSERT_MSG(parent->m_partitioned_dev_count == 1, "Ramdisk device should have only one partitioned device!");
  ASSERT_MSG(parent->m_devs == device, "Ramdisk partition mismatch!");

  if (!parent)
    return nullptr;

  if ((parent->m_flags & GDISKDEV_FLAG_RAM) == 0)
    return nullptr;

  const size_t partition_size = ALIGN_UP(device->m_end_lba - device->m_start_lba, SMALL_PAGE_SIZE);
  vnode_t* node = create_generic_vnode(mountpoint, VN_MOUNT | VN_FS);

  node->m_device = device;

  __tar_create_superblock(node, device);

  if (parent->m_flags & GDISKDEV_FLAG_RAM_COMPRESSED) {

    size_t decompressed_size = cram_find_decompressed_size(device);

    ASSERT_MSG(decompressed_size, "Got a decompressed_size of zero!");

    /* 
     * We need to allocate for the decompressed size 
     * TODO: set this region to read-only after the decompress is done
     */
    TAR_BLOCK_START(node) = (void*)Must(__kmem_kernel_alloc_range(decompressed_size, KMEM_CUSTOMFLAG_GET_MAKE, 0));
    TAR_SUPERBLOCK(node).m_total_blocks = decompressed_size;

    /* Is enforcing success here a good idea? */
    Must(cram_decompress(device, TAR_BLOCK_START(node)));

    ASSERT_MSG(TAR_BLOCK_START(node) != nullptr, "decompressing resulted in NULL");

    /* Free the pages of the compressed ramdisk */
    Must(__kmem_kernel_dealloc(device->m_start_lba, GET_PAGECOUNT(TAR_SUPERBLOCK(node).m_free_blocks)));

    device->m_start_lba = (uintptr_t)TAR_BLOCK_START(node);
    device->m_end_lba = (uintptr_t)TAR_BLOCK_START(node) + partition_size;
  }

  node->m_ops = &ramfs_vnode_ops;

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
EXPORT_DRIVER_PTR(ramfs);

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

#include "dev/core.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/cramfs/compression.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "oss/core.h"
#include "oss/node.h"
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

#define TAR_SUPERBLOCK(node) ((fs_oss_node_t*)oss_node_unwrap(node)) 
#define TAR_BLOCK_START(node) (TAR_SUPERBLOCK(node)->m_fs_priv)

#define TAR_USTAR_ALIGNMENT 512
#define TAR_USTAR_SIG       "ustar"

static uintptr_t decode_tar_ascii(uint8_t* str, size_t length)
{
  
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

static uintptr_t apply_tar_alignment(uintptr_t val)
{
  return (((val + TAR_USTAR_ALIGNMENT - 1) / TAR_USTAR_ALIGNMENT) + 1) * TAR_USTAR_ALIGNMENT;
}

static int ramfs_read(oss_node_t* node, void* buffer, size_t size, uintptr_t offset)
{
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
  memcpy(buffer, (void*)((uint64_t)file->m_buffer + offset), target_size);

  /* Adjust the read size */
  *size = target_size;

  return 0;
}

int tar_file_write(file_t* file, void* buffer, size_t* size, uint64_t offset)
{
  return -1;
}

int tar_file_close(file_t* file)
{
  /* Nothing to be done */
  return 0;
}

file_ops_t tar_file_ops = {
  .f_read = tar_file_read,
  .f_write = tar_file_write,
  .f_close = tar_file_close,
  0,
};

static oss_obj_t* ramfs_find(oss_node_t* node, const char* name) 
{
  tar_file_t current_file = { 0 };
  fs_oss_node_t* fsnode;
  uintptr_t current_offset = 0;
  size_t name_len = strlen(name);

  if (!name || !name_len)
    return nullptr;

  fsnode = oss_node_unwrap(node);

  /* Create file early to catch node grabbing issues early */
  file_t* file = create_file(node, FILE_READONLY, name);

  if (!file)
    return nullptr;

  while (current_offset <= fsnode->m_total_blocks) {
    /* Raw read :clown: */
    int result = ramfs_read(node, &current_file, sizeof(tar_file_t), current_offset);

    if (result)
      break;

    /* Invalid TAR archive file */
    if (!memcmp(current_file.ustar, TAR_USTAR_SIG, 5))
      break;

    uintptr_t current_file_offset = (uintptr_t)TAR_BLOCK_START(node) + current_offset;
    uintptr_t filesize = decode_tar_ascii(current_file.size, 11);

    if (strcmp(name, (const char*)current_file.fn) == 0) {
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

            /* Make sure file opperations go through ramfs */
            file_set_ops(file, &tar_file_ops);

            /* Attach the object once we know that it has been found */
            ASSERT_MSG(!oss_attach_obj_rel(node, name, file->m_obj), "Failed to add object to oss node while trying to find ramfs file!");

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

  destroy_oss_obj(file->m_obj);
  return nullptr;
}

int ramfs_close(oss_node_t* node, oss_obj_t* obj)
{
  return 0;
}

int ramfs_msg(oss_node_t* node, driver_control_code_t code, void* buffer, size_t size)
{
  return 0;
}

int ramfs_destroy(oss_node_t* node)
{
  return 0;
}

static struct oss_node_ops ramfs_node_ops = {
  .f_open = ramfs_find,
  .f_close = ramfs_close,
  .f_msg = ramfs_msg,
  .f_destroy = ramfs_destroy,
};

static void __tar_create_superblock(oss_node_t* node, partitioned_disk_dev_t* device)
{
  if (!node)
    return;

  TAR_SUPERBLOCK(node)->m_device = device;
  TAR_SUPERBLOCK(node)->m_first_usable_block = device->m_start_lba;
  TAR_SUPERBLOCK(node)->m_blocksize = 1;
  /* free_blocks holds the aligned compressed size */
  TAR_SUPERBLOCK(node)->m_free_blocks = ALIGN_UP(device->m_end_lba - device->m_start_lba, SMALL_PAGE_SIZE);
  TAR_SUPERBLOCK(node)->m_total_blocks = TAR_SUPERBLOCK(node)->m_free_blocks;
  TAR_SUPERBLOCK(node)->m_dirty_blocks = 0;
  TAR_SUPERBLOCK(node)->m_max_filesize = -1;
  TAR_SUPERBLOCK(node)->m_faulty_blocks = 0;
  /* Tell the fs node where we might start (this works because we register a blocksize of 1 byte) */
  TAR_BLOCK_START(node) = (void*)device->m_start_lba;
}

int unmount_ramfs(fs_type_t* type, oss_node_t* node)
{
  partitioned_disk_dev_t* device;
  fs_oss_node_t* fs;

  fs = oss_node_getfs(node);

  device = fs->m_device;

  device->m_parent->m_flags |= GDISKDEV_FLAG_WAS_MOUNTED;
  return 0;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
oss_node_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) 
{
  /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
  const disk_dev_t* parent = device->m_parent;

  ASSERT_MSG(parent->m_partitioned_dev_count == 1, "Ramdisk device should have only one partitioned device!");
  ASSERT_MSG(parent->m_devs == device, "Ramdisk partition mismatch!");

  if (!parent)
    return nullptr;

  if ((parent->m_flags & GDISKDEV_FLAG_RAM) == 0)
    return nullptr;

  oss_node_t* node = create_fs_oss_node(mountpoint, type, &ramfs_node_ops);

  __tar_create_superblock(node, device);

  if ((parent->m_flags & GDISKDEV_FLAG_RAM_COMPRESSED) == GDISKDEV_FLAG_RAM_COMPRESSED &&
      (parent->m_flags & GDISKDEV_FLAG_WAS_MOUNTED) != GDISKDEV_FLAG_WAS_MOUNTED) {

    ASSERT_MSG(cram_is_compressed_library(device), "Library marked as a compressed library, but check failed!");

    size_t decompressed_size = cram_find_decompressed_size(device);

    ASSERT_MSG(decompressed_size, "Got a decompressed_size of zero!");

    /* 
     * We need to allocate for the decompressed size 
     * TODO: set this region to read-only after the decompress is done
     */
    TAR_BLOCK_START(node) = (void*)Must(__kmem_kernel_alloc_range(decompressed_size, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE));
    TAR_SUPERBLOCK(node)->m_total_blocks = decompressed_size;

    /* Is enforcing success here a good idea? */
    Must(cram_decompress(device, TAR_BLOCK_START(node)));

    ASSERT_MSG(TAR_BLOCK_START(node) != nullptr, "decompressing resulted in NULL");

    /* Free the pages of the compressed ramdisk */
    Must(__kmem_kernel_dealloc(device->m_start_lba, GET_PAGECOUNT(device->m_start_lba, TAR_SUPERBLOCK(node)->m_free_blocks)));

    device->m_start_lba = (uintptr_t)TAR_BLOCK_START(node);
    device->m_end_lba = (uintptr_t)TAR_BLOCK_START(node) + decompressed_size;
  }

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
  /* Oh no */
  .f_unmount = unmount_ramfs,
  .m_driver = &ramfs,
  .m_flags = FST_REQ_DRIVER
};

int ramfs_init() 
{
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

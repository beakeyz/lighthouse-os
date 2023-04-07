#include <libk/stddef.h>
#include <libk/string.h>
#include <libk/error.h>
#include <fs/core.h>
#include <fs/vnode.h>

struct ext2_superblock {

} __attribute__((packed));

struct ext2_inode_table {

} __attribute__((packed));

struct ext2_block_group_descriptor {

} __attribute__((packed));

typedef struct ext2_info {
  uint32_t blocksize;
  uint32_t inodes_per_group;
  uint32_t inode_size;
  uint32_t blocks_group_count;
} ext2_info_t;

struct ext2 {

  struct ext2_superblock*               m_superblock;
  struct ext2_block_group_descriptor*   m_block_group_descriptor;

  partitioned_disk_dev_t*               m_device;

  /* stats of this ext2 filesystem */
  ext2_info_t                           m_info;
  
  fs_ops_t                              m_ops;
};

struct ext2_superblock* fetch_superblock();

vnode_t* ext2_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  ASSERT_MSG(device, "Can't initialize ext2 fs without a disk device");

  return 0;
}

fs_type_t ext2_type = {
  .m_name = "ext2",
  .f_mount = ext2_mount,
};

void init_ext2_fs() {

  /* TODO: caches? */

  register_filesystem(&ext2_type);
}

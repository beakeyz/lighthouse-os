#ifndef __ANIVA_FS_EXT2__
#define __ANIVA_FS_EXT2__
#include <libk/stddef.h>

#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

struct ext2_inode {
  uint16_t mode;
  uint16_t uid;
  uint32_t size;
  uint32_t access_time;
  uint32_t create_time;
  uint32_t mod_time;
  uint32_t deletion_time;
  uint16_t gid_low;
  uint16_t links_count;
  uint32_t blocks;
  uint32_t flags;

  /* Os dependant field, we just mark as reserved */
  uint32_t reserved1;

  uint32_t block[EXT2_N_BLOCKS];
  uint32_t generation;
  uint32_t file_acl;
  uint32_t faddr;
  
  /* Second OS dependant field, we use linux here */
  uint8_t frag_num;
  uint8_t frag_size;
  uint16_t padding1;
  uint16_t uid_high;
  uint16_t gid_high;
  uint32_t reserved2;
};

typedef struct ext2_superblock {
  uint32_t inodes_count;
  uint32_t blk_count;
  uint32_t reserved_blk_count;
  uint32_t free_blk_count;
  uint32_t free_inode_count;
  uint32_t first_data_blk;
  uint32_t log_blk_size;
  uint32_t log_frag_size;
  uint32_t blks_per_group;
  uint32_t frags_per_group;
  uint32_t inodes_per_group;
  uint32_t mnt_time;
  uint32_t wrt_time;
  uint16_t mnt_count;
  uint16_t max_mnt_count;
  uint16_t sb_magic;
  uint16_t fs_state;
  uint16_t errors;
  uint16_t minor_rev_level;
  uint32_t lastcheck_time;
  uint32_t check_interval;
  uint32_t original_os;
  uint32_t rev_level;
  uint32_t default_res_uid;
  uint32_t default_res_gid;

  union {
    struct {
      uint32_t first_inode;
      uint16_t inode_size;
      uint16_t block_group_nr;
      uint32_t feature_compat;
      uint32_t feature_incompat;
      uint32_t feature_ro_compat;
      uint8_t uuid[16];
      char volume_name[16];
      char last_mounted[64];
      uint32_t alg_usage_bitmap;
    } dynamic_rev;
    struct {
      uint64_t res0;
      uint64_t res1;
      uint32_t res2;
      uint8_t res3[96];
      uint32_t res4;
    } orig_rev;
  };

  uint8_t prealloc_blks;
  uint8_t prealloc_dir_blks;
  uint16_t padding1;

  uint8_t journal_uuid[16];
  uint32_t journal_inum;
  uint32_t journal_dev;
  uint32_t last_orphan;
  uint32_t hash_seed[4];
  uint8_t def_hash_version;
  uint8_t res_char_pad;
  uint16_t res_word_pad;
  uint32_t default_mnt_opts;
  uint32_t first_meta_bg;

  uint32_t padding2[190];
} __attribute__((packed)) ext2_superblock_t;

/* Behaviour for errors */
#define EXT2_ERRORS_CONTINUE    1
#define EXT2_ERRORS_REMOUNT_RO  2
#define EXT2_ERRORS_PANIC       3
#define EXT2_ERRORS_DEFAULT     EXT2_ERRORS_CONTINUE

/* OS codes, from linux */
#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OS_MASIX		2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4

/* Rev levels */
#define EXT2_ORIG_REV       0
#define EXT2_DYN_REV        1

#define EXT2_ORIG_IND_SIZE  128

struct ext2_inode_table {

} __attribute__((packed));

struct ext2_block_group_descriptor {

} __attribute__((packed));

void init_ext2_fs();

#endif // !__ANIVA_FS_EXT2__

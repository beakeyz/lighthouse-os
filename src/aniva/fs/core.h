#ifndef __ANIVA_FS_CORE__
#define __ANIVA_FS_CORE__

#include <libk/stddef.h>
#include <libk/flow/error.h>
#include <dev/disk/generic.h>

#define FS_DEFAULT_ROOT_MP  "Root"
#define FS_INITRD_MP        "Initrd"

struct oss_node;
struct oss_node_ops;
struct aniva_driver;

typedef struct fs_type {
  const char* m_name;
  uint32_t m_flags;

  int (*f_unmount)(struct fs_type*, struct oss_node*);
  struct oss_node* (*f_mount)(struct fs_type*, const char*, partitioned_disk_dev_t* dev);

  struct aniva_driver* m_driver;

  struct fs_type* m_next;
} fs_type_t;

#define FST_REQ_DRIVER (0x00000001)

void init_fs_core();

fs_type_t* get_fs_driver(fs_type_t* fs);
fs_type_t* get_fs_type(const char* name);

ErrorOrPtr register_filesystem(fs_type_t* fs);
ErrorOrPtr unregister_filesystem(fs_type_t* fs);

typedef struct fs_oss_node {
  fs_type_t* m_type;
  partitioned_disk_dev_t* m_device;

  uint32_t m_blocksize;
  uint32_t m_flags;

  /* FIXME: are these fields supposed to to be 64-bit? */
  uint32_t m_dirty_blocks;
  uint32_t m_faulty_blocks;
  size_t m_free_blocks;
  size_t m_total_blocks;

  uintptr_t m_first_usable_block;
  uintptr_t m_max_filesize;

  void* m_fs_priv;
} fs_oss_node_t;

#define oss_node_getfs(node) ((fs_oss_node_t*)oss_node_unwrap((node)))

struct oss_node* create_fs_oss_node(const char* name, struct oss_node_ops* ops);
void destroy_fs_oss_node(struct oss_node* node);

#endif // !__ANIVA_FS_CORE__

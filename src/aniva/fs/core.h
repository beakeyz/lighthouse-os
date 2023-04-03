#ifndef __ANIVA_FS_CORE__
#define __ANIVA_FS_CORE__

#include <libk/stddef.h>
#include <libk/error.h>

struct aniva_driver;

typedef struct fs_type {
  const char* m_name;
  uint32_t m_flags;

  int (*f_mount)(struct fs_type*, const char*, void*);

  struct fs_type* m_next;

  struct aniva_driver* m_driver;
} fs_type_t;

#define FST_REQ_DRIVER (0x00000001)

void init_fs_core();

fs_type_t* get_fs_driver(fs_type_t* fs);
ErrorOrPtr register_filesystem(fs_type_t* fs);
ErrorOrPtr unregister_filesystem(fs_type_t* fs);
fs_type_t* get_fs_type(const char* name);

#endif // !__ANIVA_FS_CORE__

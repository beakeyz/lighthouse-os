#ifndef __ANIVA_FS_DIR__
#define __ANIVA_FS_DIR__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct oss_node;
struct oss_obj;
struct dir;

typedef struct dir_ops {
  int (*f_create_child)(struct dir*, const char* name);
  int (*f_close) (struct dir*);
} dir_ops_t;

typedef struct dir {
  struct dir_ops* ops;
  struct oss_node* node;

  const char* name;

  size_t mtime;
  size_t ctime;
  size_t atime;

  uint32_t children;
  uint32_t flags;
  uint32_t size;

  uint32_t priv_size;
  void* priv;
} dir_t;

dir_t* create_dir(struct oss_node* parent, uint32_t flags, const char* name);
void destroy_dir(dir_t* dir);

int dir_create_child(dir_t* dir, const char* name);

dir_t* dir_open(const char* path);
kerror_t dir_close(dir_t* dir);

#endif // !__ANIVA_FS_DIR__

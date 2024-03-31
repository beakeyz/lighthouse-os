#ifndef __ANIVA_FS_DIR__
#define __ANIVA_FS_DIR__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct oss_node;
struct oss_obj;
struct dir;

typedef struct dir_ops {
  int               (*f_destroy) (struct dir*);
  int               (*f_create_child)(struct dir*, const char* name);
  int               (*f_remove_child)(struct dir*, const char* name);
  struct oss_obj*   (*f_find)(struct dir*, const char* path);
  struct oss_obj*   (*f_read)(struct dir*, uint64_t idx);
} dir_ops_t;

typedef struct dir {
  struct oss_node* node;
  struct dir_ops* ops;

  const char* name;

  size_t mtime;
  size_t ctime;
  size_t atime;

  uint32_t child_capacity;
  uint32_t flags;
  uint32_t size;

  void* priv;
} dir_t;

dir_t* create_dir(struct oss_node* root, const char* path, struct dir_ops* ops, void* priv, uint32_t flags);
void destroy_dir(dir_t* dir);

int dir_create_child(dir_t* dir, const char* name);
int dir_remove_child(dir_t* dir, const char* name);

struct oss_obj* dir_find(dir_t* dir, const char* path);
struct oss_obj* dir_read(dir_t* dir, uint64_t idx);

dir_t* dir_open(const char* path);
kerror_t dir_close(dir_t* dir);

#endif // !__ANIVA_FS_DIR__

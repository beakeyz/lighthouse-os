#ifndef __ANIVA_FS_DIR__
#define __ANIVA_FS_DIR__

#include "libk/flow/error.h"
#include "oss/object.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct dir;
struct file;
struct oss_object;

typedef struct dir_ops {
    int (*f_destroy)(struct dir*);
    int (*f_create_child)(struct dir*, struct oss_object* child);
    int (*f_remove_child)(struct dir*, const char* name);
    struct oss_object* (*f_find_idx)(struct dir*, uint64_t idx);
    struct oss_object* (*f_find)(struct dir*, const char* path);
} dir_ops_t;

typedef struct dir {
    struct oss_object* object;
    struct dir_ops* ops;

    const char* key;

    size_t mtime;
    size_t ctime;
    size_t atime;

    uint32_t child_capacity;
    uint32_t flags;
    uint32_t size;

    mutex_t* lock;

    /* Private field for directory implementors */
    void* priv;
} dir_t;

dir_t* create_dir(const char* key, struct dir_ops* ops, void* priv, uint32_t flags);

int dir_do_attach(dir_t* dir, oss_object_t* parent);

int dir_create_child(dir_t* dir, oss_object_t* child);
int dir_remove_child(dir_t* dir, oss_object_t* child);

struct oss_object* dir_find(dir_t* dir, const char* path);
struct oss_object* dir_find_idx(dir_t* dir, uint64_t idx);

dir_t* dir_open(const char* path);
dir_t* dir_open_from(struct oss_object* rel, const char* path);
kerror_t dir_close(dir_t* dir);

#endif // !__ANIVA_FS_DIR__

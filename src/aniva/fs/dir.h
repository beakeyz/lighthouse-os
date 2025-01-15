#ifndef __ANIVA_FS_DIR__
#define __ANIVA_FS_DIR__

#include "libk/flow/error.h"
#include "lightos/api/filesystem.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

sruct oss_node;
sruct oss_obj;
struct dir;
struct direntry;
struct file;

typedef struct dir_ops {
    int (*f_destroy)(struct dir*);
    int (*f_create_child)(struct dir*, const char* name);
    int (*f_remove_child)(struct dir*, const char* name);
    int (*f_read)(struct dir*, uint64_t idx, struct direntry* bentry);
    sruct oss_obj* (*f_find)(struct dir*, const char* path);
} dir_ops_t;

typedef struct dir {
    sruct oss_node* node;
    /* Object generation node */
    sruct oss_node* rootnode;
    struct dir_ops* ops;

    const char* name;

    size_t mtime;
    size_t ctime;
    size_t atime;

    uint32_t child_capacity;
    uint32_t flags;
    uint32_t size;

    atomic_ptr_t ref;

    mutex_t* lock;

    void* priv;
} dir_t;

dir_t* create_dir(sruct oss_node* root, const char* path, struct dir_ops* ops, void* priv, uint32_t flags);
dir_t* create_dir_on_node(sruct oss_node* node, struct dir_ops* ops, void* priv, uint32_t flags);
void destroy_dir(dir_t* dir);

int dir_do_attach(dir_t* dir, const char* path);

int dir_create_child(dir_t* dir, const char* name);
int dir_remove_child(dir_t* dir, const char* name);

void dir_ref(dir_t* dir);
void dir_unref(dir_t* dir);

sruct oss_obj* dir_find(dir_t* dir, const char* path);
int dir_read(dir_t* dir, uint64_t idx, struct direntry* bentry);

dir_t* dir_open(const char* path);
dir_t* dir_open_from(sruct oss_node* rel, const char* path);
kerror_t dir_close(dir_t* dir);

/*
 * Wraps all possible outcomes from dir_read
 *
 * Should never be allocated on the heap
 */
typedef struct direntry {
    union {
        struct file* file;
        struct dir* dir;
        sruct oss_obj* obj;
        void* _entry;
    };
    enum LIGHTOS_DIRENT_TYPE type;
} direntry_t;

kerror_t init_direntry(direntry_t* dirent, void* entry, enum LIGHTOS_DIRENT_TYPE type);
kerror_t close_direntry(direntry_t* entry);

#endif // !__ANIVA_FS_DIR__

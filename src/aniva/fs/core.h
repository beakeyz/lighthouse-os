#ifndef __ANIVA_FS_CORE__
#define __ANIVA_FS_CORE__

#include "dev/disk/volume.h"
#include "lightos/api/filesystem.h"
#include "oss/object.h"
#include <libk/flow/error.h>
#include <libk/stddef.h>

struct dir;
struct dir_ops;
struct aniva_driver;
struct fs_root_object;

#define FS_DEFAULT_ROOT_MP "Root"

typedef struct fs_type {
    const char* m_name;
    uint32_t m_flags;
    enum LIGHTOS_FSTYPE fstype;

    int (*f_unmount)(struct fs_type*, oss_object_t*);
    oss_object_t* (*f_mount)(struct fs_type*, const char*, volume_t* dev);

    struct aniva_driver* m_driver;

    struct fs_type* m_next;
} fs_type_t;

#define FST_REQ_DRIVER (0x00000001)

void init_fs_core();

fs_type_t* get_fs_driver(fs_type_t* fs);
fs_type_t* get_fs_type(const char* name);

kerror_t register_filesystem(fs_type_t* fs);
kerror_t unregister_filesystem(fs_type_t* fs);

/*
 * Filesystem root object
 *
 * This guy is created once when a filesystem is mounted. It keeps track of the filesystem-
 * specific information in one place. Every filesystem-based object has a reference to the
 * root object, since they all need to know stuff about their filesystem.
 *
 * When a filesystem is unmounted, all downstream references are dropped, since unmounting
 * is a forcefull opperation. We need to see if we can kill the responsible (non-kernel)
 * processes when they still hold filesystem object references when a fsroot is unmounted
 */
typedef struct fs_root_object {
    fs_type_t* m_type;
    volume_t* m_device;

    uint32_t m_blocksize;
    uint32_t m_flags;

    /* FIXME: are these fields supposed to to be 64-bit? */
    uint32_t m_dirty_blocks;
    uint32_t m_faulty_blocks;
    size_t m_free_blocks;
    size_t m_total_blocks;

    uintptr_t m_first_usable_block;
    uintptr_t m_max_filesize;

    /* Directory at the fs root */
    struct dir* rootdir;

    void* m_fs_priv;
} fs_root_object_t;

fs_root_object_t* create_fs_root_object(const char* name, fs_type_t* type, struct dir_ops* fsroot_ops, void* dir_priv);
void destroy_fsroot_object(fs_root_object_t* object);

fs_root_object_t* oss_object_get_fsobj(oss_object_t* object);

error_t fsroot_mount(oss_object_t* mountpoint, fs_root_object_t* object);
error_t fsroot_unmount(fs_root_object_t* object);

#endif // !__ANIVA_FS_CORE__

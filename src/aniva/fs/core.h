#ifndef __ANIVA_FS_CORE__
#define __ANIVA_FS_CORE__

#include "dev/disk/volume.h"
#include "oss/node.h"
#include <libk/flow/error.h>
#include <libk/stddef.h>

#define FS_DEFAULT_ROOT_MP "Root"
#define FS_INITRD_MP "Initrd"

struct oss_node_ops;
struct aniva_driver;

typedef struct fs_type {
    const char* m_name;
    uint32_t m_flags;

    int (*f_unmount)(struct fs_type*, oss_node_t*);
    struct oss_node* (*f_mount)(struct fs_type*, const char*, volume_t* dev);

    struct aniva_driver* m_driver;

    struct fs_type* m_next;
} fs_type_t;

#define FST_REQ_DRIVER (0x00000001)

void init_fs_core();

fs_type_t* get_fs_driver(fs_type_t* fs);
fs_type_t* get_fs_type(const char* name);

kerror_t register_filesystem(fs_type_t* fs);
kerror_t unregister_filesystem(fs_type_t* fs);

typedef struct fs_oss_node {
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

    void* m_fs_priv;
} fs_oss_node_t;

static inline fs_oss_node_t* oss_node_unwrap_to_fsnode(oss_node_t* node)
{
    while (node->type != OSS_OBJ_GEN_NODE && !node->priv)
        node = node->parent;

    if (!node)
        return nullptr;

    return (fs_oss_node_t*)node->priv;
}

#define oss_node_getfs(node) (oss_node_unwrap_to_fsnode((node)))

struct oss_node* create_fs_oss_node(const char* name, fs_type_t* type, struct oss_node_ops* ops);
void destroy_fs_oss_node(struct oss_node* node);

#endif // !__ANIVA_FS_CORE__

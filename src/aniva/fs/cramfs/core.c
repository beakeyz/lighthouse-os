#include "dev/core.h"
#include "dev/disk/device.h"
#include "dev/disk/volume.h"
#include "dev/driver.h"
#include "fs/cramfs/compression.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/api/filesystem.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include <fs/core.h>

int ramfs_init();
int ramfs_exit();

typedef struct tar_file {
    uint8_t fn[100];
    uint8_t mode[8];
    uint8_t owner_id[8];
    uint8_t gid[8];

    uint8_t size[12];
    uint8_t m_time[12];

    uint8_t checksum[8];

    uint8_t type;
    uint8_t lnk[100];

    uint8_t ustar[6];
    uint8_t version[2];

    uint8_t owner[32];
    uint8_t group[32];

    uint8_t d_maj[8];
    uint8_t d_min[8];

    uint8_t prefix[155];
} __attribute__((packed)) tar_file_t;

typedef uint8_t* tar_block_start_t;

typedef enum TAR_TYPE {
    TAR_TYPE_FILE = '0',
    TAR_TYPE_HARD_LINK = '1',
    TAR_TYPE_SYMLINK = '2',
    TAR_TYPE_CHAR_DEV = '3',
    TAR_TYPE_BLK_DEV = '4',
    TAR_TYPE_DIR = '5',
    TAR_TYPE_FIFO = '6',
} TAR_TYPE_t;

#define TAR_SUPERBLOCK(node) ((fs_root_object_t*)oss_node_unwrap(node))
#define TAR_BLOCK_START(__fsroot) ((__fsroot)->m_fs_priv)

#define TAR_USTAR_ALIGNMENT 512
#define TAR_USTAR_SIG "ustar"

static uintptr_t decode_tar_ascii(uint8_t* str, size_t length)
{
    uintptr_t ret = 0;
    uint8_t* c = str;

    /* Itterate to null byte */
    for (uintptr_t i = 0; i < length; i++) {
        ret *= 8;
        ret += *c - '0';
        c++;
    }

    return ret;
}

static uintptr_t apply_tar_alignment(uintptr_t val)
{
    return (((val + TAR_USTAR_ALIGNMENT - 1) / TAR_USTAR_ALIGNMENT) + 1) * TAR_USTAR_ALIGNMENT;
}

static int ramfs_read(fs_root_object_t* fsroot, void* buffer, size_t size, uintptr_t offset)
{
    if (!fsroot || !buffer || !size)
        return -1;

    uintptr_t start_offset = (uintptr_t)TAR_BLOCK_START(fsroot);

    memcpy(buffer, (void*)(start_offset + offset), size);

    return 0;
}

static int tar_file_read(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
    size_t target_size;

    if (!file || !buffer || !size)
        return -1;

    if (!(*size))
        return -2;

    target_size = *size;
    *size = 0;

    if (offset >= file->m_total_size)
        return -3;

    /* Make sure we never read outside of the file */
    if (offset + target_size > file->m_total_size)
        target_size -= (offset + target_size) - file->m_total_size;

    /* ->m_buffer points to the actual data inside ramfs, so just copy it out of the ro region */
    memcpy(buffer, (void*)((uint64_t)file->m_buffer + offset), target_size);

    /* Adjust the read size */
    *size = target_size;

    return 0;
}

int tar_file_write(file_t* file, void* buffer, size_t* size, uint64_t offset)
{
    return 0;
}

int tar_file_close(file_t* file)
{
    /* Nothing to be done */
    return 0;
}

file_ops_t tar_file_ops = {
    .f_read = tar_file_read,
    .f_write = tar_file_write,
    .f_close = tar_file_close,
    0,
};

oss_object_t* ramfs_find(dir_t* dir, const char* path)
{
    tar_file_t current_file = { 0 };
    fs_root_object_t* fsnode;
    uintptr_t current_offset = 0;
    size_t name_len = strlen(path);
    size_t abs_path_len = 0;

    if (!path || !name_len)
        return nullptr;

    /* Grab the fsroot of this directory */
    fsnode = dir->fsroot;

    while (current_offset <= fsnode->m_total_blocks) {
        /* Raw read :clown: */
        int result = ramfs_read(fsnode, &current_file, sizeof(tar_file_t), current_offset);

        if (result)
            break;

        /* Invalid TAR archive file */
        if (!memcmp(current_file.ustar, TAR_USTAR_SIG, 5))
            break;

        uintptr_t current_file_offset = (uintptr_t)TAR_BLOCK_START(fsnode) + current_offset;
        uintptr_t filesize = decode_tar_ascii(current_file.size, 11);

        if (current_file.type != TAR_TYPE_FILE && current_file.type != TAR_TYPE_DIR)
            goto cycle;

        if (name_len > sizeof(current_file.fn))
            name_len = sizeof(current_file.fn);

        /*
         * If we have a private field (which cramfs uses to store the absolute path of a dir, relative
         * to the mountpoint object), compute the string length
         */
        if (dir->priv)
            abs_path_len = strlen((char*)dir->priv);

        /* If this is a entry that's not relative to this dir, skip it */
        if (dir->priv && strncmp(dir->priv, (const char*)current_file.fn, abs_path_len) != 0)
            goto cycle;

        // KLOG_DBG("cramfs fn: %s (on dir: %s)\n", current_file.fn, dir->priv ? dir->priv : "<Root>");

        if (strncmp(path, (const char*)&current_file.fn[abs_path_len], name_len) == 0) {
            /*
             * TODO: how do we want this to work?
             * TODO: Create vobj
             */
            switch (current_file.type) {
            case TAR_TYPE_FILE: {
                uint8_t* data = (uint8_t*)(current_file_offset + TAR_USTAR_ALIGNMENT);

                /* Create file early to catch node grabbing issues early */
                file_t* file = create_file(FILE_READONLY, path);

                if (!file)
                    return nullptr;

                /* We can just fill the files data, since it is readonly */
                file->m_buffer = data;
                file->m_buffer_size = filesize;
                file->m_total_size = filesize;
                file->m_buffer_offset = current_file_offset;

                /* Make sure file opperations go through ramfs */
                file_set_ops(file, &tar_file_ops);

                /* Attach the object once we know that it has been found */
                // ASSERT_MSG(!oss_attach_obj_rel(node, name, file->m_obj), "Failed to add object to oss node while trying to find ramfs file!");

                /* Export the file. No need to connect this guy anywhere, since that will be done by the oss subsystem */
                return file->m_obj;
            }
            case TAR_TYPE_DIR: {
                char* dir_fn = strdup((const char*)current_file.fn);
                dir_t* new_dir = create_dir(path, dir->ops, dir->fsroot, dir_fn, NULL);

                if (!new_dir)
                    return nullptr;

                return new_dir->object;
            }
            default:
                kernel_panic("cramfs: unsupported fsentry type");
            }
        }

    cycle:
        current_offset += apply_tar_alignment(filesize);
    }

    return nullptr;
}

static oss_object_t* ramfs_dir_read(dir_t* dir, uint64_t idx)
{
    return 0;
}

static int ramfs_dir_destroy(dir_t* dir)
{
    /* We put the absolute path of a directory at ->priv */
    if (dir->priv)
        kfree((void*)dir->priv);

    return 0;
}

dir_ops_t ramfs_dir_ops = {
    .f_destroy = ramfs_dir_destroy,
    .f_open = ramfs_find,
    .f_open_idx = ramfs_dir_read,
};

static void __tar_init_superblock(fs_root_object_t* fsroot, volume_t* device)
{
    fsroot->m_device = device;
    fsroot->m_first_usable_block = device->info.min_offset;
    fsroot->m_blocksize = 1;
    /* free_blocks holds the aligned compressed size */
    fsroot->m_free_blocks = ALIGN_UP(device->info.max_offset - device->info.min_offset, SMALL_PAGE_SIZE);
    fsroot->m_total_blocks = fsroot->m_free_blocks;
    fsroot->m_dirty_blocks = 0;
    fsroot->m_max_filesize = -1;
    fsroot->m_faulty_blocks = 0;
    /* Tell the fs node where we might start (this works because we register a blocksize of 1 byte) */
    TAR_BLOCK_START(fsroot) = (void*)device->info.min_offset;
}

int unmount_ramfs(fs_type_t* type, oss_object_t* node)
{
    volume_t* device;
    fs_root_object_t* fs;

    fs = oss_object_get_fsobj(node);

    device = fs->m_device;

    /* Correct volume flags */
    device->flags &= ~VOLUME_FLAG_SYSTEMIZED;
    device->flags |= VOLUME_FLAG_HAD_SYSTEM;
    return 0;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
oss_object_t* mount_ramfs(fs_type_t* type, const char* mountpoint, volume_t* device)
{
    kerror_t error;
    size_t decompressed_size;
    fs_root_object_t* fsroot;
    /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
    const volume_device_t* parent = device->parent;

    ASSERT_MSG(parent->nr_volumes == 1, "Ramdisk device should have only one partitioned device!");
    ASSERT_MSG(parent->vec_volumes == device, "Ramdisk partition mismatch!");

    if (!parent)
        return nullptr;

    /* This volume device can't possibly be a ramdisk */
    if ((parent->flags & VOLUME_DEV_FLAG_MEMORY) != VOLUME_DEV_FLAG_MEMORY)
        return nullptr;

    /* Create the fsroot */
    fsroot = create_fs_root_object(mountpoint, type, &ramfs_dir_ops, NULL);

    if (!fsroot)
        return nullptr;

    /* Initialize the 'superblock' */
    __tar_init_superblock(fsroot, device);

    /* Check if we need to decompress this volume */
    if ((device->flags & VOLUME_FLAG_COMPRESSED) == VOLUME_FLAG_COMPRESSED && (device->flags & VOLUME_FLAG_HAD_SYSTEM) != VOLUME_FLAG_HAD_SYSTEM) {

        ASSERT_MSG(cram_is_compressed_library(device), "Library marked as a compressed library, but check failed!");

        decompressed_size = cram_find_decompressed_size(device);

        ASSERT_MSG(decompressed_size, "Got a decompressed_size of zero!");

        /*
         * We need to allocate for the decompressed size
         * TODO: set this region to read-only after the decompress is done
         *
         * NOTE: This is a long-term allocation. This needs to persist, even after the volume is unmounted or some shit
         */
        error = kmem_kernel_alloc_range(&TAR_BLOCK_START(fsroot), decompressed_size, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE);

        if (error)
            goto dealloc_and_exit;

        fsroot->m_total_blocks = decompressed_size;

        /* Is enforcing success here a good idea? */
        error = cram_decompress(device, TAR_BLOCK_START(fsroot));

        if (error)
            goto dealloc_and_exit;

        ASSERT_MSG(TAR_BLOCK_START(fsroot) != nullptr, "decompressing resulted in NULL");

        /* Free the pages of the compressed ramdisk */
        kmem_kernel_dealloc(device->info.min_offset, GET_PAGECOUNT(device->info.min_offset, fsroot->m_free_blocks));

        device->info.min_offset = (uintptr_t)TAR_BLOCK_START(fsroot);
        device->info.max_offset = (uintptr_t)TAR_BLOCK_START(fsroot) + decompressed_size;

        /* Reinit the superblock */
        __tar_init_superblock(fsroot, device);
    }

    /*
     * When mounting a filesytem, the root object is always seen as a directory. This
     * is the directory that gets the first reference to the fsroot
     */
    return fsroot->rootdir->object;

dealloc_and_exit:

    /* Fuck mann */
    if (TAR_BLOCK_START(fsroot))
        kmem_kernel_dealloc((vaddr_t)TAR_BLOCK_START(fsroot), decompressed_size);

    destroy_fsroot_object(fsroot);
    return nullptr;
}

aniva_driver_t ramfs = {
    .m_name = "cramfs",
    .m_type = DT_FS,
    .f_init = ramfs_init,
    .f_exit = ramfs_exit,
};
EXPORT_DRIVER_PTR(ramfs);

fs_type_t cramfs = {
    .m_name = "cramfs",
    .f_mount = mount_ramfs,
    /* Oh no */
    .f_unmount = unmount_ramfs,
    .m_driver = &ramfs,
    .m_flags = FST_REQ_DRIVER,
    .fstype = LIGHTOS_FSTYPE_CRAMFS,
};

int ramfs_init()
{
    int ret = 0;
    kerror_t error = register_filesystem(&cramfs);

    if (error) {
        kernel_panic("Failed to register filesystem");
        ret = -1;
    }

    return ret;
}

int ramfs_exit()
{

    /*
     * TODO: unregister
     */

    return 0;
}

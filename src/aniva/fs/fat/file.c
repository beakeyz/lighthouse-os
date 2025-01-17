#include "file.h"
#include "fs/core.h"
#include "fs/dir.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/heap.h"

static int fat_read(file_t* file, void* buffer, size_t* p_size, uintptr_t offset)
{
    size_t size;
    fs_root_object_t* fsroot;

    if (!p_size)
        return -1;

    fsroot = file->fsroot;

    size = *p_size;

    if (offset >= file->m_total_size)
        return -2;

    /* Trim the size a bit if it happens to overflows */
    if ((offset + size) > file->m_total_size)
        size -= ((offset + size) - file->m_total_size);

    /* Update this shit */
    *p_size = size;

    return fat32_read_clusters(fsroot, buffer, file->m_private, offset, size);
}

static int fat_write(file_t* file, void* buffer, size_t* p_size, uintptr_t offset)
{
    if (!file || !file->m_obj)
        return -KERR_INVAL;

    return fat32_write_clusters(file->fsroot, buffer, file->m_private, offset, *p_size);
}

static int fat_sync(file_t* file)
{
    if (!file || !file->m_obj)
        return -KERR_INVAL;

    /* FIXME: This currently flushes the entire FATFS xD */
    return fatfs_flush(file->fsroot);
}

static int fat_close(file_t* file)
{
    fat_file_t* fat_file;

    fat_file = file->m_private;

    destroy_fat_file(fat_file);
    return 0;
}

file_ops_t fat_file_ops = {
    .f_close = fat_close,
    .f_read = fat_read,
    .f_write = fat_write,
    .f_sync = fat_sync,
};

int fat_dir_destroy(dir_t* dir)
{
    fat_file_t* ffile;

    if (!dir)
        return -KERR_INVAL;

    ffile = dir->priv;

    destroy_fat_file(ffile);
    return 0;
}

int fat_dir_create_child(dir_t* dir, oss_object_t* child)
{
    kernel_panic("TODO: fat_dir_create_child");
}

int fat_dir_remove_child(dir_t* dir, const char* name)
{
    kernel_panic("TODO: fat_dir_remove_child");
}

static void fat_8_3_to_filename(char* fat8_3, char* b_name, uint32_t bsize)
{
    uint32_t idx = 0;
    uint32_t out_idx = 0;

    memset(b_name, 0, bsize);

    /* First, copy over the filename before the period */
    while (fat8_3[idx] != ' ' && idx < 11) {
        b_name[out_idx++] = fat8_3[idx++];
    }

    /* If we reached the end, stop here */
    if (idx == 11)
        return;

    /* Search where the file extention starts */
    while (fat8_3[idx] == ' ' && idx < 11)
        idx++;

    if (idx == 11)
        return;

    /* Copy the period */
    b_name[out_idx++] = '.';

    /* Copy the extention */
    while (idx < 11) {
        b_name[out_idx++] = fat8_3[idx++];
    }
}

oss_object_t* fat_dir_read(dir_t* dir, uint64_t idx)
{
    fat_dir_entry_t entry;
    file_t* f = NULL;
    dir_t* d = NULL;
    char namebuf[256] = { NULL };

    if (!KERR_OK(fat32_read_dir_entry(dir->priv, &entry, namebuf, sizeof(namebuf), idx, NULL)))
        return nullptr;

    /* If the first byte in the name buffer is still null, we could not find any lfn entries for this index... */
    if (!namebuf[0])
        fat_8_3_to_filename((char*)entry.name, namebuf, sizeof(namebuf));

    if ((entry.attr & FAT_ATTR_DIR) == FAT_ATTR_DIR)
        d = create_fat_dir(GET_FAT_FSINFO(dir->fsroot), NULL, namebuf);
    else
        f = create_fat_file(GET_FAT_FSINFO(dir->fsroot), NULL, namebuf);

    if (d)
        return d->object;

    if (f)
        return f->m_obj;

    return nullptr;
}

dir_ops_t fat_dir_ops = {
    .f_destroy = fat_dir_destroy,
    .f_connect_child = fat_dir_create_child,
    .f_disconnect_child = fat_dir_remove_child,
    .f_open_idx = fat_dir_read,
    .f_open = __fat_open,
};

dir_ops_t* get_fat_dir_ops()
{
    return &fat_dir_ops;
}

/*!
 * @brief: Create a file that adheres to the FAT fs
 *
 */
file_t* create_fat_file(fat_fs_info_t* info, uint32_t flags, const char* path)
{
    file_t* ret;
    fat_file_t* ffile;

    ret = nullptr;
    ffile = nullptr;

    /* Invaid args */
    if (!info || !path || !path[0])
        return nullptr;

    ret = create_file(flags, path);

    if (!ret)
        return nullptr;

    /* This will make both the fat file and the regular file aware of each other */
    ffile = allocate_fat_file(ret, FFILE_TYPE_FILE);

    if (!ffile)
        goto fail_and_exit;

    file_set_ops(ret, &fat_file_ops);

    return ret;
fail_and_exit:
    oss_object_close(ret->m_obj);
    return nullptr;
}

/*!
 * @brief: Create a directory object that adheres to the FAT fs
 */
dir_t* create_fat_dir(fat_fs_info_t* info, uint32_t flags, const char* path)
{
    dir_t* ret;
    fat_file_t* ffile;

    ret = create_dir(path, &fat_dir_ops, info->fsroot, NULL, NULL);

    if (!ret)
        return nullptr;

    ffile = allocate_fat_file(ret, FFILE_TYPE_DIR);

    if (!ffile)
        goto dealloc_and_exit;

    return ret;
dealloc_and_exit:
    dir_close(ret);
    return nullptr;
}

void destroy_fat_file(fat_file_t* file)
{
    if (file->clusterchain_buffer)
        kfree(file->clusterchain_buffer);

    /* If the file was a directory, this is allocated on the kernel heap */
    if (file->dir_entries)
        kfree(file->dir_entries);

    deallocate_fat_file(file);
}

/*!
 * @brief: Compute the size of a FAT file
 *
 * We count the amount of clusters inside its clusterchain buffer
 * NOTE: we'll need to keep this buffer updated, meaning that we need to reallocate it
 * every time we write (and sync) this file
 */
size_t get_fat_file_size(fat_file_t* file)
{
    fs_root_object_t* fsroot;

    if (!file || !file->parent)
        return NULL;

    switch (file->type) {
    case FFILE_TYPE_FILE:
        fsroot = file->file_parent->fsroot;
        break;
    case FFILE_TYPE_DIR:
        fsroot = file->dir_parent->fsroot;
        break;
    }

    return file->clusters_num * GET_FAT_FSINFO(fsroot)->cluster_size;
}

kerror_t fat_file_update_dir_entries(fat_file_t* file)
{
    size_t fsize;
    size_t n_actual_dirent = 0;
    size_t n_raw_dirent;
    fat_dir_entry_t* tmp_buffer;
    fat_dir_entry_t* tmp_dest_buffer;

    if (!file || file->type != FFILE_TYPE_DIR)
        return -KERR_INVAL;

    if (file->dir_entries)
        kfree(file->dir_entries);

    fsize = get_fat_file_size(file);
    n_raw_dirent = fsize / sizeof(fat_dir_entry_t);

    /* Allocate buffer nr. 1 */
    tmp_buffer = kmalloc(fsize);

    /* FUCKK */
    if (!tmp_buffer)
        return -KERR_NOMEM;

    /* Okay, buffer nr. 2 */
    tmp_dest_buffer = kmalloc(fsize);

    /* SHIT */
    if (!tmp_dest_buffer) {
        kfree(tmp_buffer);
        return -KERR_NOMEM;
    }

    /* Load all the data into the buffer */
    if (!KERR_OK(fat32_read_clusters(file->dir_parent->fsroot, (uint8_t*)tmp_buffer, file, 0, fsize)))
        return -KERR_IO;

    for (uint64_t i = 0; i < n_raw_dirent; i++) {
        fat_dir_entry_t* entry = &tmp_buffer[i];

        /* End */
        if (entry->name[0] == 0x00)
            break;

        if (entry->attr & FAT_ATTR_VOLUME_ID && entry->attr != FAT_ATTR_LFN)
            continue;

        /* Copy this entry into the destination buffer */
        memcpy(&tmp_dest_buffer[n_actual_dirent], entry, sizeof(*entry));
        n_actual_dirent++;
    }

    if (!n_actual_dirent)
        goto free_and_exit;

    /* Reallocate */
    file->dir_entries = kmalloc(n_actual_dirent * sizeof(fat_dir_entry_t));
    file->n_direntries = n_actual_dirent;

    /* Yikes */
    if (!file->dir_entries)
        goto free_and_exit;

    /* Copy the actual entries into the directories buffer */
    memcpy(file->dir_entries, tmp_dest_buffer, n_actual_dirent * sizeof(fat_dir_entry_t));

free_and_exit:
    /* Clear this buffer */
    kfree(tmp_buffer);

    /* Clear this buffer as well */
    kfree(tmp_dest_buffer);
    return 0;
}

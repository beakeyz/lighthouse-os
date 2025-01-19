#include "dev/core.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/dir.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/fat/file.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "lightos/api/filesystem.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/path.h"
#include <libk/stddef.h>

/* Driver predefinitions */
int fat32_exit();
int fat32_init();

enum FAT_ERRCODES {
    FAT_OK = 0,
    FAT_INVAL = 1,
};

#define TO_UPPERCASE(c) ((c >= 'a' && c <= 'z') ? c - 0x20 : c)
#define TO_LOWERCASE(c) ((c >= 'A' && c <= 'Z') ? c + 0x20 : c)

typedef uintptr_t fat_offset_t;

/* TODO: */
// static int fat_parse_lfn(void* dir, fat_offset_t* offset);
// static int fat_parse_short_fn(fat_dir_entry_t* dir, const char* name);

static int parse_fat_bpb(fat_boot_sector_t* bpb, fs_root_object_t* fsroot)
{
    if (!fat_valid_bs(bpb)) {
        return FAT_INVAL;
    }

    if (!bpb->fats || !bpb->reserved_sectors) {
        return FAT_INVAL;
    }

    if (!POWER_OF_2(bpb->sector_size) || bpb->sector_size < 512 || bpb->sector_size > SMALL_PAGE_SIZE) {
        return FAT_INVAL;
    }

    if (!POWER_OF_2(bpb->sectors_per_cluster)) {
        return FAT_INVAL;
    }

    if (bpb->sectors_per_fat16 == 0 && bpb->fat32.sectors_per_fat == 0) {
        return FAT_INVAL;
    }

    /* TODO: fill data in superblock with useful data */
    fsroot->m_blocksize = bpb->sector_size;

    return FAT_OK;
}

/*!
 * @brief Load a singular clusters value
 *
 * @fsroot: the filesystem object to opperate on
 * @buffer: the buffer for the clusters value
 * @cluster: the 'index' of the cluster on disk
 */
static int
fat32_load_cluster(fs_root_object_t* fsroot, uint32_t* buffer, uint32_t cluster)
{
    int error;
    fat_fs_info_t* info;

    info = GET_FAT_FSINFO(fsroot);
    error = fatfs_read(fsroot, buffer, sizeof(uint32_t), info->usable_sector_offset + (cluster * sizeof(uint32_t)));

    if (error)
        return error;

    /* Mask any fucky bits to comply with FAT32 standards */
    *buffer &= 0x0fffffff;

    return 0;
}

/*!
 * @brief: Set a singular clusters value
 *
 * @node: the filesystem object to opperate on
 * @buffer: the buffer for the clusters value
 * @cluster: the 'index' of the cluster on disk
 */
/* static */ int
fat32_set_cluster(fs_root_object_t* fsroot, uint32_t buffer, uint32_t cluster)
{
    int error;
    fat_fs_info_t* info;

    /* Mask any fucky bits to comply with FAT32 standards */
    buffer &= 0x0fffffff;

    info = GET_FAT_FSINFO(fsroot);
    error = fatfs_write(fsroot, &buffer, sizeof(uint32_t), info->usable_sector_offset + (cluster * sizeof(uint32_t)));

    if (error)
        return error;

    return 0;
}

/*!
 * @brief: Caches the cluster chain for a file
 *
 * The file is responsible for managing the buffer after this point
 */
static int fat32_cache_cluster_chain(fs_root_object_t* fsroot, fat_file_t* file, uint32_t start_cluster)
{
    int error;
    size_t length;
    uint32_t buffer;

    if (start_cluster < 0x2 || start_cluster > FAT_ENT_EOF)
        return -1;

    if (!file)
        return -2;

    length = 1;
    buffer = start_cluster;

    if (file->clusterchain_buffer)
        return -3;

    /* Count the clusters */
    while (true) {
        error = fat32_load_cluster(fsroot, &buffer, buffer);

        if (error)
            return error;

        if (buffer < 0x2 || buffer >= FAT_ENT_EOF)
            break;

        length++;
    }

    /* Set correct lengths */
    file->clusters_num = length;

    if (!length)
        return 0;

    /* Reset buffer */
    buffer = start_cluster;

    /* Allocate chain */
    file->clusterchain_buffer = kmalloc(length * sizeof(uint32_t));

    /* Loop and store the clusters */
    for (uint32_t i = 0; i < length; i++) {
        file->clusterchain_buffer[i] = buffer;

        error = fat32_load_cluster(fsroot, &buffer, buffer);

        if (error)
            return error;
    }

    return 0;
}

/*!
 * @brief: Write one entrie cluster to disk
 *
 * The size of @buffer must be at least ->cluster_size
 * this size is assumed
 */
static int
__fat32_write_cluster(fs_root_object_t* fsroot, void* buffer, uint32_t cluster)
{
    int error;
    fat_fs_info_t* info;
    uintptr_t current_cluster_offset;

    if (!fsroot || !fsroot->m_fs_priv || cluster < 2)
        return -1;

    info = GET_FAT_FSINFO(fsroot);
    current_cluster_offset = (info->usable_clusters_start + (cluster - 2) * info->boot_sector_copy.sectors_per_cluster) * info->boot_sector_copy.sector_size;
    error = fatfs_write(fsroot, buffer, info->cluster_size, current_cluster_offset);

    if (error)
        return -2;

    return 0;
}

int fat32_read_clusters(fs_root_object_t* fsroot, uint8_t* buffer, struct fat_file* file, uint32_t start, size_t size)
{
    int error;
    uintptr_t current_index;
    uintptr_t current_offset;
    uintptr_t current_deviation;
    uintptr_t current_delta;
    uintptr_t current_cluster_offset;
    uintptr_t index;
    fat_fs_info_t* info;

    if (!fsroot)
        return -1;

    info = GET_FAT_FSINFO(fsroot);

    index = 0;

    while (index < size) {
        current_offset = start + index;
        current_index = current_offset / info->cluster_size;
        current_deviation = current_offset % info->cluster_size;

        current_delta = size - index;

        /* Limit the delta to the size of a cluster, except if we try to get an unaligned size smaller than 'cluster_size' */
        if (current_delta > info->cluster_size - current_deviation)
            current_delta = info->cluster_size - current_deviation;

        current_cluster_offset = (info->usable_clusters_start + (file->clusterchain_buffer[current_index] - 2) * info->boot_sector_copy.sectors_per_cluster)
            * info->boot_sector_copy.sector_size;

        error = fatfs_read(fsroot, buffer + index, current_delta, current_cluster_offset + current_deviation);

        if (error)
            return -2;

        index += current_delta;
    }

    return 0;
}

/*!
 * @brief: Finds the index of a free cluster
 *
 * @returns: 0 on success, anything else on failure (TODO: real ierror API)
 * @buffer: buffer where the index gets put into
 */
static int
__fat32_find_free_cluster(fs_root_object_t* fsroot, uint32_t* buffer)
{
    int error;
    uint32_t current_index;
    uint32_t cluster_buff;
    fat_fs_info_t* info;

    if (!fsroot)
        return -KERR_INVAL;

    info = GET_FAT_FSINFO(fsroot);

    /* Start at index 2, cuz the first 2 clusters are weird lol */
    current_index = 2;
    cluster_buff = 0;

    while (current_index < info->cluster_count) {

        /* Try to load the value of the cluster at our current index into the value buffer */
        error = fat32_load_cluster(fsroot, &cluster_buff, current_index);

        if (error)
            return error;

        /* Zero in a cluster index means its free / unused */
        if (cluster_buff == 0)
            break;

        current_index++;
    }

    /* Could not find a free cluster, WTF */
    if (current_index >= info->cluster_count)
        return -1;

    /* Place the index into the buffer */
    *buffer = current_index;

    return 0;
}

static uint32_t __fat32_dir_entry_get_start_cluster(fat_dir_entry_t* e)
{
    return ((uint32_t)e->first_cluster_low & 0xffff) | ((uint32_t)e->first_cluster_hi << 16);
}

/*!
 * @brief: Get the first cluster entry from a file
 */
static inline uint32_t __fat32_file_get_start_cluster_entry(fat_file_t* ff)
{
    if (!ff->clusterchain_buffer)
        return 0;

    return ff->clusterchain_buffer[0];
}

/*!
 * @brief: Find the last cluster of a fat directory entry
 *
 * @returns: 0 on success, anything else on failure
 */
static int
__fat32_find_end_cluster(fs_root_object_t* fsroot, uint32_t start_cluster, uint32_t* buffer)
{
    int error;
    uint32_t previous_value;
    uint32_t value_buffer;

    value_buffer = start_cluster;
    previous_value = value_buffer;

    while (true) {
        /* Load one cluster */
        error = fat32_load_cluster(fsroot, &value_buffer, value_buffer);

        if (error)
            return error;

        /* Reached end? */
        if (value_buffer < 0x2 || value_buffer >= BAD_FAT32)
            break;

        previous_value = value_buffer;
    }

    /* previous_value holds the index to the last cluster at this point. Ez pz */
    *buffer = previous_value;

    return 0;
}

int fat32_write_clusters(fs_root_object_t* fsroot, uint8_t* buffer, struct fat_file* ffile, uint32_t offset, size_t size)
{
    int error;
    bool did_overflow;
    uint32_t max_offset;
    uint32_t overflow_delta;
    uint32_t overflow_clusters;
    uint32_t write_clusters;
    uint32_t write_start_cluster;
    uint32_t start_cluster_index;
    uint32_t cluster_internal_offset;
    uint32_t file_start_cluster;
    uint32_t file_end_cluster;
    fat_fs_info_t* fs_info;
    file_t* file;

    if (!size || !fsroot)
        return -KERR_INVAL;

    file = ffile->parent;

    if (!file)
        return -1;

    fs_info = GET_FAT_FSINFO(fsroot);
    max_offset = ffile->clusters_num * fs_info->cluster_size - 1;
    overflow_delta = 0;

    if (offset + size > max_offset)
        overflow_delta = (offset + size) - max_offset;

    did_overflow = false;

    /* Calculate how many clusters we might need to allocate extra */
    overflow_clusters = ALIGN_UP(overflow_delta, fs_info->cluster_size) / fs_info->cluster_size;
    /* Calculate in how many clusters we need to write */
    write_clusters = ALIGN_UP(size, fs_info->cluster_size) / fs_info->cluster_size;
    /* Calculate the first cluster where we need to write */
    write_start_cluster = ALIGN_DOWN(offset, fs_info->cluster_size) / fs_info->cluster_size;
    /* Calculate the start offset within the first cluster */
    cluster_internal_offset = offset % fs_info->cluster_size;
    /* Find the starting cluster */
    file_start_cluster = __fat32_file_get_start_cluster_entry(ffile);

    /* Find the last cluster of this file */
    error = __fat32_find_end_cluster(fsroot, file_start_cluster, &file_end_cluster);

    if (error)
        return error;

    uint32_t next_free_cluster;

    /* Allocate the overflow clusters */
    while (overflow_clusters) {
        next_free_cluster = NULL;

        /* Find a free cluster */
        error = __fat32_find_free_cluster(fsroot, &next_free_cluster);

        if (error)
            return error;

        /* Add this cluster to the end of this files chain */
        fat32_set_cluster(fsroot, next_free_cluster, file_end_cluster);
        fat32_set_cluster(fsroot, EOF_FAT32, next_free_cluster);

        did_overflow = true;
        file_end_cluster = next_free_cluster;
        overflow_clusters--;
    }

    /* Only re-cache the cluster chain if we made changes to the clusters */
    if (did_overflow) {
        /* Reset cluster_chain data for this file */
        kfree(ffile->clusterchain_buffer);
        ffile->clusterchain_buffer = NULL;
        ffile->clusters_num = NULL;

        /* Re-cache the cluster chain */
        error = fat32_cache_cluster_chain(fsroot, ffile, file_start_cluster);

        if (error)
            return error;

        file->m_total_size = ffile->clusters_num * fs_info->cluster_size;
    }

    /*
     * We can now perform a nice write opperation on this files data
     */
    start_cluster_index = write_start_cluster / ffile->clusters_num;
    uint8_t* cluster_buffer = kmalloc(fs_info->cluster_size);

    /* Current offset: the offset into the current cluster */
    uint32_t current_offset = cluster_internal_offset;
    /* Current delta: how much have we already written */
    uint32_t current_delta = 0;
    /* Write size: how many bytes are in this write */
    uint32_t write_size;

    /* FIXME: this is prob slow asf. We could be smarter about our I/O */
    for (uint32_t i = 0; i < write_clusters; i++) {

        if (current_delta >= size)
            break;

        /* This is the cluster we need to write shit to */
        uint32_t this_cluster = ffile->clusterchain_buffer[start_cluster_index + i];

        /* Figure out the write size */
        write_size = size - current_delta;

        /* Write size exceeds the size of our clusters */
        if (write_size > fs_info->cluster_size - current_offset)
            write_size = fs_info->cluster_size - current_offset;

        /* Read the current content into our buffer here */
        fat32_read_clusters(fsroot, cluster_buffer, ffile, this_cluster, 1);

        /* Copy the bytes over */
        memcpy(&cluster_buffer[current_offset], &buffer[current_delta], write_size);

        /* And write back the changes we've made */
        __fat32_write_cluster(fsroot, cluster_buffer, this_cluster);

        current_delta += write_size;
        /* After the first write, any subsequent write will always start at the start of a cluster */
        current_offset = 0;
    }

    fat_dir_entry_t* dir_entry = (fat_dir_entry_t*)((uintptr_t)cluster_buffer + ffile->direntry_offset);

    if (fat32_read_clusters(fsroot, cluster_buffer, ffile, ffile->direntry_cluster, 1))
        goto free_and_exit;

    /* Add the size to this file */
    dir_entry->size += size;

    __fat32_write_cluster(fsroot, cluster_buffer, ffile->direntry_cluster);

free_and_exit:
    kfree(cluster_buffer);
    return 0;
}

/*!
 * @brief: Parse a single lfn entry
 *
 * Every lfn is able to hold 13 Unicode characters.
 */
static inline void fat_lfn_parse(fat_lfn_entry_t* lfn, char lfn_buffer[255], u32* p_lfn_len)
{
    u32 write_idx;
    u32 ort_idx = (lfn->idx_ord & ~FAT_LFN_LAST_ENTRY);

    /* Invalid index */
    if (!ort_idx || ort_idx > FAT_MAX_NAME_SLOTS)
        return;

    /* The index we need to write at in the lfn_buffer */
    write_idx = (ort_idx - 1) * 13;

    /* Parse the first name field */
    for (u32 i = 0; i < 5 && lfn->name_0[i] && lfn->name_0[i] != 0xffff; i++)
        lfn_buffer[write_idx++] = (char)lfn->name_0[i];

    /* Parse the second name field */
    for (u32 i = 0; i < 6 && lfn->name_1[i] && lfn->name_1[i] != 0xffff; i++)
        lfn_buffer[write_idx++] = (char)lfn->name_1[i];

    /* Parse the third name field */
    for (u32 i = 0; i < 2 && lfn->name_2[i] && lfn->name_2[i] != 0xffff; i++)
        lfn_buffer[write_idx++] = (char)lfn->name_2[i];

    /* Export the largest write index */
    if (p_lfn_len && write_idx > *p_lfn_len)
        *p_lfn_len = write_idx;
}

/*!
 * @brief: Reads a dir entry from a directory at index @idx
 */
int fat32_read_dir_entry(fat_file_t* dir, fat_dir_entry_t* out, char* namebuf, u32 namelen, uint32_t idx, uint32_t* diroffset)
{
    fat_dir_entry_t* c_entry;

    if (!out)
        return -1;

    if (dir->type != FFILE_TYPE_DIR)
        return -KERR_INVAL;

    if (!dir->dir_parent || !dir->dir_entries)
        return -KERR_INVAL;

    /* Loop over all entries in the directory */
    for (u32 i = 0; i < dir->n_direntries; i++) {
        c_entry = &dir->dir_entries[i];

        if (c_entry->name[0] == 0)
            break;

        /* Parse the lfn entry if we find it */
        if (c_entry->attr == FAT_ATTR_LFN) {
            fat_lfn_parse((fat_lfn_entry_t*)c_entry, namebuf, NULL);
            continue;
        }

        if (c_entry->attr & FAT_ATTR_VOLUME_ID)
            continue;

        if (!idx) {
            *out = dir->dir_entries[i];

            if (diroffset)
                *diroffset = i * sizeof(fat_dir_entry_t);

            return 0;
        }

        memset(namebuf, 0, namelen);
        idx--;
    }

    return -KERR_NOT_FOUND;
}

static void
transform_fat_filename(char* dest, const char* src)
{
    uint32_t i = 0;
    uint32_t src_dot_idx = 0;

    bool found_ext = false;

    // put spaces
    memset(dest, ' ', 11);

    while (src[src_dot_idx + i]) {
        // limit exceed
        if (i >= 11 || (i >= 8 && !found_ext)) {
            return;
        }

        // we have found the '.' =D
        if (src[i] == '.') {
            found_ext = true;
            src_dot_idx = i + 1;
            i = 0;
        }

        if (found_ext) {
            dest[8 + i] = TO_UPPERCASE(src[src_dot_idx + i]);
            i++;
        } else {
            dest[i] = TO_UPPERCASE(src[i]);
            i++;
        }
    }
}

static int
fat32_open_dir_entry(fs_root_object_t* fsroot, fat_dir_entry_t* current, fat_dir_entry_t* out, char* rel_path, uint32_t* diroffset)
{
    /* We always use this buffer if we don't support lfn */
    char transformed_buffer[12] = { 0 };
    char lfn_buffer[256] = { 0 };
    int error;
    u32 cluster_num;
    u32 lfn_len = 0;
    u32 path_len;

    size_t dir_entries_count;
    size_t clusters_size;
    fat_dir_entry_t* dir_entries;

    fat_fs_info_t* p;
    fat_file_t tmp_ffile = { 0 };

    if (!out || !rel_path)
        return -1;

    path_len = strlen(rel_path);

    /* Make sure we open a directory, not a file */
    if ((current->attr & FAT_ATTR_DIR) == 0)
        return -2;

    /* Get our filesystem info and cluster number */
    p = GET_FAT_FSINFO(fsroot);
    cluster_num = current->first_cluster_low | ((uint32_t)current->first_cluster_hi << 16);

    /* Cache the cluster chain into the temp file */
    error = fat32_cache_cluster_chain(fsroot, &tmp_ffile, cluster_num);

    if (error)
        goto fail_dealloc_cchain;

    /* Calculate the size of the data */
    clusters_size = tmp_ffile.clusters_num * p->cluster_size;
    dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

    /* Allocate for that size */
    dir_entries = kmalloc(clusters_size);

    /* Load all the data into the buffer */
    error = fat32_read_clusters(fsroot, (uint8_t*)dir_entries, &tmp_ffile, 0, clusters_size);

    if (error)
        goto fail_dealloc_dir_entries;

    /* Everything is going great, now we can transform the pathname for our scan */
    transform_fat_filename(transformed_buffer, rel_path);

    /* Loop over all the directory entries and check if any paths match ours */
    for (uint32_t i = 0; i < dir_entries_count; i++) {
        fat_dir_entry_t entry = dir_entries[i];

        /* End */
        if (entry.name[0] == 0x00) {
            error = -3;
            break;
        }

        if (entry.attr == FAT_ATTR_LFN) {
            /* TODO: support */
            fat_lfn_parse((fat_lfn_entry_t*)&entry, lfn_buffer, &lfn_len);

            error = -4;
            continue;
        }

        if (entry.attr & FAT_ATTR_VOLUME_ID)
            continue;

        /* This our file/directory? */
        if ((lfn_len && lfn_len == path_len && strncmp(rel_path, lfn_buffer, path_len) == 0) || strncmp(transformed_buffer, (const char*)entry.name, 11) == 0) {
            *out = entry;

            /* Give out where we found this bitch */
            if (diroffset)
                *diroffset = i * sizeof(fat_dir_entry_t);

            error = 0;
            break;
        }

        memset(lfn_buffer, 0, sizeof(lfn_buffer));
        lfn_len = 0;
    }

fail_dealloc_dir_entries:
    kfree(dir_entries);
fail_dealloc_cchain:
    kfree(tmp_ffile.clusterchain_buffer);
    return error;
}

/*!
 * @brief: Open a file direntry on the fat filesystem
 */
oss_object_t* __fat_open(dir_t* dir, const char* path)
{
    int error;
    file_t* ret;
    fat_file_t* fat_file;
    fat_fs_info_t* info;
    fat_dir_entry_t current;
    fs_root_object_t* fsroot;
    oss_path_t oss_path;

    u32 direntry_cluster;
    u32 direntry_offset;

    if (!dir || !dir->fsroot || !path)
        return nullptr;

    fsroot = dir->fsroot;
    info = GET_FAT_FSINFO(fsroot);
    /* Copy the root entry copy =D */
    current = info->root_entry_cpy;

    oss_parse_path(path, &oss_path);

    for (uintptr_t i = 0; i < oss_path.n_subpath; i++) {

        /* Skip invalid subpaths */
        if (!oss_path.subpath_vec[i])
            continue;

        /* Respect skip tokens */
        if (oss_path.subpath_vec[i][0] == _OSS_PATH_SKIP)
            continue;

        /* Pre-cache the starting offset of the direntry we're searching in */
        direntry_cluster = __fat32_dir_entry_get_start_cluster(&current);

        /* Find the directory entry */
        error = fat32_open_dir_entry(fsroot, &current, &current, oss_path.subpath_vec[i], &direntry_offset);

        /* A single directory may have entries spanning over multiple clusters */
        direntry_cluster += direntry_offset / info->cluster_size;
        direntry_offset %= info->cluster_size;

        if (error)
            break;
    }

    /* Kill the path we don't need anymore */
    oss_destroy_path(&oss_path);

    if (error)
        return nullptr;

    kernel_panic("[TODO] FATFS: Implement both dir and file opening");

    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.attr & (FAT_ATTR_DIR | FAT_ATTR_VOLUME_ID)) == FAT_ATTR_DIR)
        return nullptr;

    /* Create the fat directory once we've found it exists */
    ret = create_fat_file(info, NULL, path);

    if (!ret)
        return nullptr;

    fat_file = ret->m_private;

    if (!fat_file)
        return nullptr;

    /* Make sure the file knows about its cluster chain */
    error = fat32_cache_cluster_chain(fsroot, fat_file, (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16)));

    if (error)
        goto destroy_and_exit;

    ret->m_total_size = get_fat_file_size(fat_file);
    ret->m_logical_size = current.size;

    /* Cache the offset of the clusterchain */
    fat_file->clusterchain_offset = (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16));
    fat_file->direntry_offset = direntry_offset;
    fat_file->direntry_cluster = direntry_cluster;

    return ret->m_obj;

destroy_and_exit:
    oss_object_close(ret->m_obj);
    return nullptr;
}

/*
 * FAT will keep a list of vobjects that have been given away by the system, in order to map
 * vobjects to disk-locations and FATs easier.
 */
oss_object_t* fat32_mount(fs_type_t* type, const char* mountpoint, volume_t* device)
{
    /* Get FAT =^) */
    fs_root_object_t* fsroot;
    fat_fs_info_t* ffi;
    uint8_t buffer[device->info.logical_sector_size];
    fat_boot_sector_t* boot_sector;
    fat_boot_fsinfo_t* internal_fs_info;

    KLOG_DBG("Trying to mount FAT32\n");

    /* Try and read the first block from this volume */
    if (!volume_bread(device, 0, buffer, 1))
        return nullptr;

    /* Create root node. This guy simply takes in the same dir ops as any other fat dir */
    fsroot = create_fs_root_object(mountpoint, type, get_fat_dir_ops(), NULL);

    ASSERT_MSG(fsroot, "Failed to create fs oss node for FAT fs");

    /* Initialize the fs private info */
    create_fat_info(fsroot, device);

    /* Cache superblock and fs info */
    ffi = GET_FAT_FSINFO(fsroot);

    /* Set the local pointer to the bootsector */
    boot_sector = &ffi->boot_sector_copy;

    /*
     * Copy the boot sector cuz why not lol
     * NOTE: the only access the boot sector after this point lmao
     */
    memcpy(boot_sector, buffer, sizeof(fat_boot_sector_t));

    /*
     * Create a cache for our sectors
     * NOTE: cache_count being NULL means we want the default number of cache entries
     */
    ffi->sector_cache = create_fat_sector_cache(device->info.max_transfer_sector_nr * device->info.logical_sector_size, NULL);

    /* Try to parse boot sector */
    int parse_error = parse_fat_bpb(boot_sector, fsroot);

    /* Not a valid FAT fs */
    if (parse_error != FAT_OK)
        goto fail;

    /* Check FAT type */
    if (!boot_sector->sectors_per_fat16 && boot_sector->fat32.sectors_per_fat) {
        /* FAT32 */
        ffi->fat_type = FTYPE_FAT32;
    } else {
        /* FAT12 or FAT16 */
        ffi->fat_type = FTYPE_FAT16;
        goto fail;
    }

    /* Attempt to reset the blocksize for the partitioned device */
    if (device->info.logical_sector_size > fsroot->m_blocksize) {

        // if (!pd_set_blocksize(device, oss_node_getfs(node)->m_blocksize))
        kernel_panic("FAT: Failed to set blocksize! abort");
    }

    /* Did a previous OS mark this filesystem as dirty? */
    ffi->has_dirty = is_fat32(ffi) ? (boot_sector->fat32.state & FAT_STATE_DIRTY) : (boot_sector->fat16.state & FAT_STATE_DIRTY);

    /* TODO: move superblock initialization to its own function */

    /* Fetch blockcounts */
    fsroot->m_total_blocks = ffi->boot_sector_copy.sector_count_fat16;

    if (!fsroot->m_total_blocks)
        fsroot->m_total_blocks = ffi->boot_sector_copy.sector_count_fat32;

    fsroot->m_first_usable_block = ffi->boot_sector_copy.reserved_sectors;

    /* Precompute some useful data (TODO: compute based on FAT type)*/
    ffi->root_directory_sectors = (boot_sector->root_dir_entries * 32 + (boot_sector->sector_size - 1)) / boot_sector->sector_size;
    ffi->total_reserved_sectors = boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->fat32.sectors_per_fat) + ffi->root_directory_sectors;
    ffi->total_usable_sectors = fsroot->m_total_blocks - ffi->total_reserved_sectors;
    ffi->cluster_count = ffi->total_usable_sectors / boot_sector->sectors_per_cluster;
    ffi->cluster_size = boot_sector->sectors_per_cluster * boot_sector->sector_size;
    ffi->usable_sector_offset = boot_sector->reserved_sectors * boot_sector->sector_size;
    ffi->usable_clusters_start = (boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->fat32.sectors_per_fat));

    ffi->root_entry_cpy = (fat_dir_entry_t) {
        .name = "/",
        .first_cluster_low = boot_sector->fat32.root_cluster & 0xffff,
        .first_cluster_hi = (boot_sector->fat32.root_cluster >> 16) & 0xffff,
        .attr = FAT_ATTR_DIR,
        0
    };

    /* NOTE: we reuse the buffer of the boot sector */
    if (!volume_bread(device, 1, buffer, 1))
        goto fail;

    /* Set the local pointer */
    internal_fs_info = &ffi->boot_fs_info;

    /* Copy the boot fsinfo to the ffi structure */
    memcpy(internal_fs_info, buffer, sizeof(fat_boot_fsinfo_t));

    fsroot->m_free_blocks = ffi->boot_fs_info.free_clusters;

    KLOG_DBG("Mounted FAT32 filesystem\n");

    return fsroot->rootdir->object;
fail:

    if (fsroot) {
        destroy_fat_info(fsroot);
        /* NOTE: total node destruction is not yet done */
        destroy_fsroot_object(fsroot);
    }

    return nullptr;
}

/*!
 * @brief: Unmount the FAT filesystem
 *
 * This simply needs to free up the private structures we created for this node
 * to hold the information about the FAT filesystem. We don't need to wory about fat
 * files here, since they will be automatically killed when the filesystem is destroyed
 *
 * NOTE: An unmount should happen after a total filesystem sync, to prevent loss of data
 */
int fat32_unmount(fs_type_t* type, oss_object_t* object)
{
    fs_root_object_t* fsroot;

    if (!type || !object)
        return -EINVAL;

    fsroot = oss_object_get_fsobj(object);

    if (!fsroot)
        return -EINVAL;

    destroy_fat_info(fsroot);
    return 0;
}

aniva_driver_t fat32_drv = {
    .m_name = "fat32",
    .m_descriptor = "FAT32 filesystem driver",
    .m_type = DT_FS,
    .m_version = DRIVER_VERSION(1, 0, 0),
    .f_init = fat32_init,
    .f_exit = fat32_exit,
};
EXPORT_DRIVER_PTR(fat32_drv);

fs_type_t fat32_type = {
    .m_driver = &fat32_drv,
    .m_name = "fat32",
    .fstype = LIGHTOS_FSTYPE_FAT32,
    .f_mount = fat32_mount,
    .f_unmount = fat32_unmount,
};

int fat32_init()
{
    kerror_t error;

    init_fat_cache();

    error = register_filesystem(&fat32_type);

    if (error)
        return -1;

    return 0;
}

int fat32_exit()
{
    kerror_t error = unregister_filesystem(&fat32_type);

    if (error)
        return -1;

    return 0;
}

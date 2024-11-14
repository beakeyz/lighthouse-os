#include "cache.h"
#include "dev/disk/volume.h"
#include "fs/core.h"
#include "fs/fat/core.h"
#include "fs/fat/file.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <mem/zalloc/zalloc.h>

#define DEFAULT_FAT_INFO_ENTRIES (8)
#define DEFAULT_FAT_SECTOR_CACHES (8)

static zone_allocator_t __fat_info_cache;
static zone_allocator_t __fat_file_cache;
static zone_allocator_t __fat_sec_entry_alloc;

struct sec_cache_entry {
    bool is_dirty : 1;
    uint32_t useage_count;
    uintptr_t block_buffer;
    uintptr_t current_block;
};

kerror_t create_fat_info(oss_node_t* node, volume_t* device)
{
    fs_oss_node_t* fsnode;
    fat_fs_info_t* info;

    if (!node)
        return -1;

    fsnode = oss_node_getfs(node);

    info = zalloc_fixed(&__fat_info_cache);

    if (!info)
        return -1;

    info->fat_lock = create_mutex(NULL);
    info->node = node;

    fsnode->m_device = device;

    fsnode->m_fs_priv = info;

    return (0);
}

void destroy_fat_info(oss_node_t* node)
{
    fs_oss_node_t* fsnode;
    fat_fs_info_t* info;

    if (!node || !GET_FAT_FSINFO(node))
        return;

    fsnode = oss_node_getfs(node);
    info = GET_FAT_FSINFO(node);

    if (info->sector_cache)
        destroy_fat_sector_cache(info->sector_cache);

    destroy_mutex(info->fat_lock);
    zfree_fixed(&__fat_info_cache, info);

    fsnode->m_fs_priv = nullptr;
}

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size, uint32_t cache_count)
{
    struct sec_cache_entry** c_entry;
    fat_sector_cache_t* cache;

    if (!block_size)
        return nullptr;

    if (!cache_count)
        cache_count = DEFAULT_FAT_SECTOR_CACHES;

    cache = kmalloc(sizeof(*cache) + (cache_count * sizeof(struct sec_cache_entry*)));

    if (!cache)
        return nullptr;

    memset(cache, 0, sizeof(*cache));

    cache->blocksize = block_size;
    cache->cache_count = cache_count;

    c_entry = cache->entries;

    if (__kmem_kernel_alloc_range((void**)&cache->block_buffers, block_size * cache_count, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE))
        goto dealloc_and_exit;

    /*
     * Initialize all the cache entries
     */
    for (uint32_t i = 0; i < cache_count; i++) {
        c_entry[i] = zalloc_fixed(&__fat_sec_entry_alloc);

        if (!c_entry[i])
            goto dealloc_and_exit;

        memset(c_entry[i], 0, sizeof(struct sec_cache_entry));

        c_entry[i]->block_buffer = (uintptr_t)&cache->block_buffers[i * block_size];

        memset((void*)c_entry[i]->block_buffer, 0, block_size);
    }

    return cache;

dealloc_and_exit:

    for (int i = 0; i < cache_count; i++)
        if (c_entry[i])
            zfree_fixed(&__fat_sec_entry_alloc, c_entry[i]);

    if (cache->block_buffers)
        __kmem_kernel_dealloc((vaddr_t)cache->block_buffers, block_size * cache_count);

    kfree(cache);
    return nullptr;
}

void destroy_fat_sector_cache(fat_sector_cache_t* cache)
{
    if (!cache)
        return;

    for (uint32_t i = 0; i < cache->cache_count; i++) {
        zfree_fixed(&__fat_sec_entry_alloc, cache->entries[i]);
    }

    __kmem_kernel_dealloc((vaddr_t)cache->block_buffers, cache->blocksize * cache->cache_count);
    kfree(cache);
}

static inline void fat_cache_use_entry(struct sec_cache_entry* index)
{
    index->useage_count++;
}

static inline int fatfs_sync_cache_entry_ex(oss_node_t* node, fat_sector_cache_t* cache, struct sec_cache_entry* entry, uint32_t logical_block_count)
{
    fs_oss_node_t* fsnode;

    if (!entry->is_dirty)
        return -1;

    fsnode = oss_node_getfs(node);

    if (volume_bwrite(fsnode->m_device, entry->current_block, (void*)entry->block_buffer, logical_block_count))
        return -1;

    entry->is_dirty = false;
    return 0;
}

static inline uint32_t get_logical_block_count(oss_node_t* node, fat_sector_cache_t* cache)
{
    fs_oss_node_t* fsnode = oss_node_getfs(node);
    return cache->blocksize / fsnode->m_device->info.logical_sector_size;
}

static inline int fatfs_sync_cache_entry(oss_node_t* node, fat_sector_cache_t* cache, struct sec_cache_entry* entry)
{
    uint32_t lbc = get_logical_block_count(node, cache);

    return fatfs_sync_cache_entry_ex(node, cache, entry, lbc);
}

static void fat_cache_find_least_used(oss_node_t* node, fat_sector_cache_t* cache, struct sec_cache_entry** entry)
{
    uint32_t i;
    struct sec_cache_entry* least_used;
    struct sec_cache_entry* c_entry;

    i = 1;
    least_used = cache->entries[0];

    /* Skip any dirty entries */
    while (least_used->is_dirty && i < cache->cache_count)
        least_used = cache->entries[i++];

    for (; i < cache->cache_count; i++) {
        c_entry = cache->entries[i];

        if (c_entry->useage_count < least_used->useage_count)
            least_used = c_entry;
    }

    if (least_used->is_dirty)
        fatfs_sync_cache_entry(node, cache, least_used);

    *entry = least_used;
}

/*!
 * @brief: Find a cache entry for a specific block index
 *
 * If we don't already have the block cached, it returns a free cache entry
 * If there are no free entries left, we replace the least used entry
 */
static int fat_cache_find_entry(oss_node_t* node, fat_sector_cache_t* cache, struct sec_cache_entry** entry, uintptr_t block)
{
    struct sec_cache_entry* c_free;
    struct sec_cache_entry* c_entry;

    c_free = nullptr;

    for (uint32_t i = 0; i < cache->cache_count; i++) {
        c_entry = cache->entries[i];

        /* If we find the block we're looking for, use this entry */
        if (c_entry->useage_count && c_entry->current_block == block) {
            *entry = c_entry;
            return 0;
        }

        if (!c_entry->useage_count && !c_free) {
            c_free = c_entry;
            continue;
        }
    }

    if (!c_free)
        fat_cache_find_least_used(node, cache, &c_free);

    *entry = c_free;
    return 0;
}

/*!
 * @brief: Read a block into the fat block buffer
 *
 * TODO: make use of more advanced caching
 * (Or put caching in the generic disk core)
 */
static int
__read(oss_node_t* node, fat_sector_cache_t* cache, struct sec_cache_entry** entry, uintptr_t block)
{
    uintptr_t offset;
    uint32_t logical_block_count;
    fs_oss_node_t* fsnode;
    struct sec_cache_entry* c_entry;

    fsnode = oss_node_getfs(node);
    offset = block * cache->blocksize;

    /* Convert to device block index */
    block = offset / fsnode->m_device->info.logical_sector_size;

    if (fat_cache_find_entry(node, cache, &c_entry, block) || !c_entry)
        return -1;

    /* Skip the block read if we alread have this block cached */
    if (c_entry->useage_count && c_entry->current_block == block)
        goto found_entry;

    logical_block_count = get_logical_block_count(node, cache);

    if (!volume_bread(fsnode->m_device, block, (void*)c_entry->block_buffer, logical_block_count))
        return -2;

found_entry:
    fat_cache_use_entry(c_entry);
    c_entry->current_block = block;

    *entry = c_entry;
    return 0;
}

/*!
 * @brief Read a bit of data from fat
 *
 * Nothing to add here...
 */
int fatfs_read(oss_node_t* node, void* buffer, size_t size, disk_offset_t offset)
{
    int error;
    fat_fs_info_t* info;
    struct sec_cache_entry* entry;
    uint64_t current_block, current_offset, current_delta;
    uint64_t lba_size, read_size;

    current_offset = 0;
    info = GET_FAT_FSINFO(node);
    lba_size = info->sector_cache->blocksize;

    while (current_offset < size) {
        current_block = (offset + current_offset) / lba_size;
        current_delta = (offset + current_offset) % lba_size;

        error = __read(node, info->sector_cache, &entry, current_block);

        if (error)
            return error;

        read_size = size - current_offset;

        /* Constantly shave off lba_size */
        if (read_size > lba_size - current_delta)
            read_size = lba_size - current_delta;

        memcpy(buffer + current_offset, &(((uint8_t*)entry->block_buffer)[current_delta]), read_size);

        current_offset += read_size;
    }

    return 0;
}

int fatfs_write(oss_node_t* node, void* buffer, size_t size, disk_offset_t offset)
{
    int error;
    fat_fs_info_t* info;
    struct sec_cache_entry* entry;
    uint64_t current_block, current_offset, current_delta;
    uint64_t lba_size, read_size;

    current_offset = 0;
    info = GET_FAT_FSINFO(node);
    lba_size = info->sector_cache->blocksize;

    while (current_offset < size) {
        current_block = (offset + current_offset) / lba_size;
        current_delta = (offset + current_offset) % lba_size;

        error = __read(node, info->sector_cache, &entry, current_block);

        if (error)
            return error;

        read_size = size - current_offset;

        /* Constantly shave off lba_size */
        if (read_size > lba_size - current_delta)
            read_size = lba_size - current_delta;

        memcpy(&(((uint8_t*)entry->block_buffer)[current_delta]), buffer + current_offset, read_size);

        /* Mark the buffer as dirty */
        entry->is_dirty = true;

        current_offset += read_size;
    }

    return 0;
}

int fatfs_flush(oss_node_t* node)
{
    int error;
    uint32_t logical_block_count;
    fat_fs_info_t* info;
    struct sec_cache_entry* entry;
    fat_sector_cache_t* cache;

    info = GET_FAT_FSINFO(node);

    if (!info)
        return -1;

    error = 0;

    cache = info->sector_cache;
    logical_block_count = get_logical_block_count(node, cache);

    for (uint32_t i = 0; i < cache->cache_count; i++) {
        entry = cache->entries[i];

        if (fatfs_sync_cache_entry_ex(node, cache, entry, logical_block_count)) {
            error = -1;
            continue;
        }
    }

    return error;
}

/*!
 * @brief: Allocates memory for a file under the FAT filesystem
 */
fat_file_t* allocate_fat_file(void* parent, enum FAT_FILE_TYPE type)
{
    fat_file_t* ret;

    ret = zalloc_fixed(&__fat_file_cache);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->parent = parent;
    ret->type = type;

    /* Make sure the file is aware of its private data */
    switch (type) {
    case FFILE_TYPE_FILE:
        ret->file_parent->m_private = ret;
        break;
    case FFILE_TYPE_DIR:
        ret->dir_parent->priv = ret;
        break;
    }

    return ret;
}

void deallocate_fat_file(fat_file_t* file)
{
    zfree_fixed(&__fat_file_cache, file);
}

/*
 * TODO: we have made a nice and fast FAT implementation in light loader, so we know
 * how fat is supposed to work a bit now. Reimplement (or actually just implement) a
 * propper caching system here to make file lookups (clusterchain reads n shit) nice
 * and fast plz =)
 */
void init_fat_cache(void)
{
    kerror_t error;

    error = init_zone_allocator(&__fat_info_cache, DEFAULT_FAT_INFO_ENTRIES * sizeof(fat_fs_info_t), sizeof(fat_fs_info_t), NULL);

    ASSERT_MSG(!(error), "Could not create FAT info cache!");

    error = init_zone_allocator(&__fat_file_cache, ZALLOC_DEFAULT_ALLOC_COUNT * sizeof(fat_file_t), sizeof(fat_file_t), NULL);

    ASSERT_MSG(!(error), "Could not create FAT file cache!");

    ASSERT_MSG(
        !(init_zone_allocator(&__fat_sec_entry_alloc, DEFAULT_FAT_SECTOR_CACHES * sizeof(struct sec_cache_entry), sizeof(struct sec_cache_entry), NULL)),
        "Failed to create a FAT sector cache allocator!");
}

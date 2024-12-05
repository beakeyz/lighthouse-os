#include "disk.h"
#include "efierr.h"
#include "efigpt.h"
#include "efilib.h"
#include "gfx.h"
#include "guid.h"
#include "heap.h"
#include "stddef.h"
#include <ctx.h>
#include <memory.h>
#include <stdio.h>

disk_dev_t* bootdevice = nullptr;

#define DISK_BUFFER_INDEX 0
#define DISK_SYSTEM_INDEX 1
#define DISK_DATA_INDEX 2

#define DISK_BUFFER_SIZE (5ul * Mib)

/* Yoink the linux data partition GUID lmao */
#define LLOADER_PART_TYPE_BASIC_GUID                       \
    {                                                      \
        0x0FC63DAF, 0x8483, 0x4772,                        \
        {                                                  \
            0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 \
        }                                                  \
    }

void register_bootdevice(disk_dev_t* device)
{
    bootdevice = device;
    device->next_partition = nullptr;
}

void register_partition(disk_dev_t* device)
{
    disk_dev_t** part;

    part = &bootdevice;

    while (*part) {
        part = &(*part)->next_partition;
    }

    *part = device;
    device->next_partition = nullptr;
}

disk_dev_t* get_bootdevice() { return bootdevice; }

int disk_init_cache(disk_dev_t* device)
{
    device->cache.cache_oldest = 0;
    device->cache.cache_dirty_flags = NULL;

    device->cache.cache_size = device->sector_size;

    for (uint8_t i = 0; i < 8; i++) {
        device->cache.cache_ptr[i] = heap_allocate(device->cache.cache_size);
    }
    return 0;
}

int disk_clear_cache(disk_dev_t* device, uint64_t block)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (device->cache.cache_block[i] != block)
            continue;

        /* If this cache entry was dirty, write it back */
        if (disk_is_cache_writes(device) && ((device->cache.cache_dirty_flags & (1 << i)) == (1 << i)) && device->cache.cache_ptr[i])
            /* Write this block back to disk */
            device->f_bwrite(device, device->cache.cache_ptr[i], 1,
                device->cache.cache_block[i]);

        /* Clear the dirty flag */
        device->cache.cache_dirty_flags &= ~(1 << i);
        device->cache.cache_usage_count[i] = NULL;
        device->cache.cache_block[i] = NULL;
        return 0;
    }

    return -1;
}

uint8_t disk_select_cache(disk_dev_t* device, uint64_t block)
{
    uint8_t lower_count = 0;
    uint8_t preferred_cache_idx = 0xff;
    uint8_t lowest_usecount = 0xFF;

    /*
     * Loop one: look for unused blocks, or blocks that are still in our
     * cache that we haven't written to yet (and flushed to disk so disk is
     * out of sync with the cache)
     */
    for (uint8_t i = 0; i < 8; i++) {

        /* Free cache? Mark it and go next */
        if ((device->cache.cache_dirty_flags & (1 << i)) != (1 << i)) {
            preferred_cache_idx = i;
            continue;
        }

        /* If there already is a cache with this block, get that one */
        if (device->cache.cache_block[i] == block)
            return i;
    }

    /* If there was a cache with a lowest count, use that */
    if (preferred_cache_idx != 0xff)
        return preferred_cache_idx;

    /*
     * Loop two: find the cache with the lowest usecount so we can use that
     * boi to store our shit
     */
    for (uint8_t i = 0; i < 8; i++) {
        if (device->cache.cache_usage_count[i] < lowest_usecount) {
            lowest_usecount = device->cache.cache_usage_count[i];
            lower_count++;
            preferred_cache_idx = i;
        }
    }

    /* NOTE: Increased lower count implies that preferred_cache_idx got set to a
     * valid index */
    if (lower_count)
        return preferred_cache_idx;

    /* Fuck: last option, use the in-house cycle to cycle to the next 'oldest'
     * cache =/ */
    preferred_cache_idx = device->cache.cache_oldest;

    device->cache.cache_oldest = (device->cache.cache_oldest + 1) % 7;

    return preferred_cache_idx;
}

int disk_flush(disk_dev_t* device)
{
    uintptr_t current_block;
    uint8_t* current_ptr;

    /* Flush any caches to disk */
    for (uint8_t i = 0; i < 8; i++) {

        /* Skip non-dirty cache entries */
        if ((device->cache.cache_dirty_flags & (1 << i)) == 0)
            continue;

        current_block = device->cache.cache_block[i];
        current_ptr = device->cache.cache_ptr[i];

        device->f_bwrite(device, current_ptr, 1, current_block);

        /* Mark this cache entry as clean */
        device->cache.cache_dirty_flags &= ~(1 << i);
    }

    /* Also flush device internals */
    device->f_flush(device);
    return 0;
}

disk_dev_t* create_and_link_partition_dev(disk_dev_t* parent,
    uintptr_t start_lba,
    uintptr_t ending_lba)
{
    disk_dev_t* ret;

    if (!parent || (parent->flags & DISK_FLAG_PARTITION) == DISK_FLAG_PARTITION)
        return nullptr;

    ret = heap_allocate(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->private = parent->private;
    ret->flags = parent->flags | DISK_FLAG_PARTITION;

    ret->optimal_transfer_factor = parent->optimal_transfer_factor;
    ret->effective_sector_size = parent->effective_sector_size;
    ret->sector_size = parent->sector_size;

    ret->first_sector = start_lba;
    ret->total_sectors = (ending_lba - start_lba);
    ret->total_size = ret->total_sectors * ret->effective_sector_size;

    /* Ops */
    ret->f_bread = parent->f_bread;
    ret->f_bwrite = parent->f_bwrite;
    ret->f_read = parent->f_read;
    ret->f_write = parent->f_write;
    ret->f_write_zero = parent->f_write_zero;

    disk_init_cache(ret);

    ret->next_partition = parent->next_partition;
    parent->next_partition = ret;

    return ret;
}

/*!
 * NOTE: ->f_bread is permitted here, since we allocate a buffer of sector_size
 * on the stack BUT: this buffer might exeed stack size so FIXME
 */
void cache_gpt_header(disk_dev_t* device)
{
    int error;

    /* LMAO */
    if (device->flags & DISK_FLAG_PARTITION)
        return;

    uint32_t lb_size = 0;
    uint32_t lb_guesses[] = {
        512,
        4096,
    };

    uint8_t buffer[sizeof(gpt_header_t)];
    gpt_header_t* header = (gpt_header_t*)buffer;

    for (size_t i = 0; i < (sizeof(lb_guesses) / sizeof(uint32_t)); i++) {

        /* Read one block */
        error = device->f_read(device, buffer, lb_guesses[i], sizeof(gpt_header_t));

        if (error)
            return;

        /* Check if the signature matches the one we need */
        if (strncmp((void*)header->signature, "EFI PART", 8))
            continue;

        lb_size = lb_guesses[i];

        break;
    }

    if (!lb_size)
        return;

    if (!device->partition_header)
        device->partition_header = heap_allocate(sizeof(gpt_header_t));

    memcpy((void*)device->partition_header, (void*)header, sizeof(*header));
}

int disk_get_partitions(disk_dev_t* device)
{
    gpt_header_t* header;

    header = device->partition_header;

    if (!header)
        return -1;

    return 0;
}

void cache_gpt_entry(disk_dev_t* device)
{
    int error;
    uint8_t buffer[device->sector_size];
    gpt_entry_t* entry;

    error = device->f_bread(device, buffer, 1, 0);

    if (error)
        return;

    entry = (gpt_entry_t*)buffer;

    for (uint64_t i = 0; i < 36 * 8; i++) {
        gfx_putchar(*((char*)entry->partition_name + i));
        if (i % 24 == 0)
            printf(" | ");
    }
}

static void gpt_entry_set_partition_name(gpt_entry_t* entry, const char* name)
{
    if (name && strlen(name) > sizeof(entry->partition_name))
        name = nullptr;

    memset(entry->partition_name, 0, sizeof(entry->partition_name));

    if (!name)
        return;

    for (uint32_t i = 0; i < strlen(name); i++) {
        entry->partition_name[i] = (uint16_t)name[i];
    }
}

/*!
 * @brief: Set the data of a particular gpt entry
 *
 * @start_offset: The start offset on disk of the partition (This will be
 * rounded down to fit the disks sector boundary)
 * @size: The size in bytes of the partition
 */
static gpt_entry_t*
disk_add_gpt_partition_entry(disk_dev_t* device, gpt_entry_t* entry,
    const char* name, uintptr_t start_offset,
    size_t size, uintptr_t attrs, guid_t guid)
{
    entry->attributes = attrs;
    entry->starting_lba = ALIGN_DOWN(start_offset, device->effective_sector_size) / device->effective_sector_size;

    if (size)
        entry->ending_lba = entry->starting_lba + (ALIGN_UP(size, device->effective_sector_size) / device->effective_sector_size);

    /*
     * Make sure to clamp the end lba
     * NOTE: the last LBA on disk is backup GPT header, so make sure to end this
     * partition before that
     */
    if (!size || entry->ending_lba >= device->total_sectors - 1)
        entry->ending_lba = device->total_sectors - 2;

    entry->unique_partition_guid = (guid_t) { .a = (uint32_t)(start_offset & 0xffffffff),
        .b = (uint16_t)(start_offset >> 32),
        .c = (uint16_t)(start_offset >> 48),
        .d = {
            0x69,
            0xAA,
            0xF5,
            0xDD,
            0x33,
            0xF5,
            0xDD,
            0x33,
        } };
    entry->partition_type_guid = guid;

    gpt_entry_set_partition_name(entry, name);
    create_and_link_partition_dev(device, entry->starting_lba, entry->ending_lba);
    return entry;
}

/*!
 * @brief: Get the needed size for a certain partition entry index
 *
 * For some entries we need to calculate how much space all the files are going
 * to take
 */
static size_t gpt_entry_get_size(uint32_t entry_index)
{
    switch (entry_index) {
    case DISK_SYSTEM_INDEX: {
        /* TODO: figure out how big the system index needs to be */
        return 0;
    }
    case DISK_DATA_INDEX: {
        /* TODO: figure out how big the data index needs to be */
        return 0;
    }
    }
    return 0;
}

static inline uintptr_t gpt_entry_get_end_offset(disk_dev_t* device,
    gpt_entry_t* entry)
{
    return (entry->ending_lba + 1) * device->effective_sector_size;
}

#define MBR_SIG 0xAA55
#define MBR_SIG_OFFSET 0x1fe
#define MBR_PARTENT_0_OFFSET 0x01BE
#define MBR_EFI_PARTITION_TYPE 0xEE
#define MBR_SECTOR_SIZE 512

struct mbr_partition_entry {
    uint8_t status;
    uint8_t first_chs[3];
    uint8_t type;
    uint8_t last_chs[3];
    uint32_t first_lba;
    uint32_t sector_num;
} __attribute__((packed));

static inline void _write_protective_mbr(disk_dev_t* device)
{
    uint16_t mbr_sig = 0xaa55;

    struct mbr_partition_entry entry = { 0 };

    entry.first_chs[0] = 0;
    entry.first_chs[1] = 2;
    entry.first_chs[2] = 0;
    memset(&entry.last_chs, 0xff, sizeof(entry.last_chs));

    entry.status = 0x80;
    entry.type = MBR_EFI_PARTITION_TYPE;
    entry.first_lba = 1;
    entry.sector_num = (uint32_t)(device->total_size >> 9);

    device->f_write_zero(device, device->effective_sector_size, 0);
    device->f_write(device, &entry, sizeof(entry), MBR_PARTENT_0_OFFSET);
    device->f_write(device, &mbr_sig, sizeof(mbr_sig), MBR_SIG_OFFSET);
}

int disk_install_partitions(disk_dev_t* device, bool add_gap)
{
    EFI_STATUS s;
    uint32_t crc_buffer;
    uint32_t partition_count;
    uint32_t total_required_size;
    uintptr_t previous_offset;
    light_ctx_t* ctx;
    void* gpt_buffer;
    gpt_header_t* header_template;
    gpt_entry_t* entry_start;
    gpt_entry_t* previous_entry;

    /* Can't install to a partition lmao */
    if ((device->flags & DISK_FLAG_PARTITION) == DISK_FLAG_PARTITION || device->total_size < (4ull * Gib))
        return -1;

    ctx = get_light_ctx();
    partition_count = 80;
    crc_buffer = 0;
    total_required_size = device->effective_sector_size + ALIGN_UP(partition_count * sizeof(gpt_entry_t), device->effective_sector_size);

    /* Prevent accidental installations */
    if (!ctx->install_confirmed)
        return -2;

    /* Allocate a buffer that we can instantly write it all to disk */
    gpt_buffer = heap_allocate(total_required_size);

    if (!gpt_buffer)
        return -3;

    memset(gpt_buffer, 0, total_required_size);

    /* The header template will be at the top of the buffer */
    header_template = (gpt_header_t*)gpt_buffer;
    /* First entry will be at the start of the next lba after the header */
    entry_start = (gpt_entry_t*)((uint8_t*)gpt_buffer + device->effective_sector_size);

    memcpy(header_template->signature, "EFI PART", 8);

    header_template->partition_entry_num = partition_count;
    header_template->partition_entry_size = sizeof(gpt_entry_t);
    /* Right after the header */
    header_template->partition_entry_lba = 2;

    header_template->first_usable_lba = 2048;
    // header_template->first_usable_lba = (ALIGN_UP(total_required_size,
    // device->effective_sector_size) / device->effective_sector_size);

    /* FIXME: When we have a secondary partition table at the end of the disk,
     * this needs to take that into a count */
    header_template->last_usable_lba = device->total_sectors - 1;

    /* GUID we yoinked from cfdisk lmao */
    uint16_t guid[] = { 0x0483, 0xfcfe, 0x84ad, 0x4181,
        0x188e, 0x768d, 0x26c1, 0xf48e };

    memcpy(&header_template->disk_guid, guid, sizeof(guid));

    /* Protective MBR should be created at lba 0 */
    header_template->my_lba = 1;
    /* NOTE: This value gives anything that checks this a seizure */
    header_template->alt_lba = header_template->last_usable_lba;

    header_template->revision = 0x00010000;
    header_template->header_size = sizeof(*header_template);

    previous_offset = header_template->first_usable_lba * device->effective_sector_size;

    if (add_gap) {
        /* Realistically, the kernel + bootloader should not take more space than
         * this */
        previous_entry = disk_add_gpt_partition_entry(device, &entry_start[DISK_BUFFER_INDEX], "LightGAP", header_template->first_usable_lba * device->effective_sector_size, (9ULL * Mib), GPT_ATTR_REQUIRED, (guid_t)LLOADER_PART_TYPE_BASIC_GUID);
        previous_offset = gpt_entry_get_end_offset(device, previous_entry);

        device->next_partition->flags &= ~(DISK_FLAG_SYS_PART | DISK_FLAG_DATA_PART);
        /* Clear the buffer partition */
        device->next_partition->f_write_zero(device->next_partition, device->next_partition->total_size, 0);
    }

    /* Realistically, the kernel + bootloader should not take more space than this
     */
    previous_entry = disk_add_gpt_partition_entry(device, &entry_start[DISK_SYSTEM_INDEX - (!add_gap)], "LightOS System", previous_offset, (3885056ULL * 512), GPT_ATTR_REQUIRED, (guid_t)EFI_PART_TYPE_EFI_SYSTEM_PART_GUID);
    previous_offset = gpt_entry_get_end_offset(device, previous_entry);
    /* Set this partition to be the system partition */
    device->next_partition->flags |= DISK_FLAG_SYS_PART;

    /* Add partition entry for system data */
    disk_add_gpt_partition_entry(device, &entry_start[DISK_DATA_INDEX - (!add_gap)], "LightOS Data", previous_offset, ALIGN_DOWN(device->total_size - previous_offset - 16 * device->effective_sector_size, device->effective_sector_size), 0, (guid_t)LLOADER_PART_TYPE_BASIC_GUID);
    /* Set this partition to be the data partition */
    device->next_partition->flags |= DISK_FLAG_DATA_PART;

    /* Create a CRC of the partition entries */
    s = BS->CalculateCrc32(entry_start, partition_count * sizeof(gpt_entry_t), &crc_buffer);

    if (EFI_ERROR(s)) {
        heap_free(gpt_buffer);
        return -4;
    }

    header_template->partition_entry_array_crc32 = crc_buffer;
    crc_buffer = 0;

    /* Create a CRC of the header */
    s = BS->CalculateCrc32(header_template, sizeof(*header_template),
        &crc_buffer);

    if (EFI_ERROR(s)) {
        heap_free(gpt_buffer);
        return -5;
    }

    header_template->header_crc32 = crc_buffer;

    _write_protective_mbr(device);

    /*
     * Write this on lba 1 =)
     * FIXME: when we only call f_write once here, we are fine and the GPT sceme
     * gets detected accross platforms. If we don't and we either do a write call
     * right before here to clear lba 0, or we go and install filesystems on the
     * created partitions, the GPT does not get detected anymore. Does this have
     * to do with some weird bug in the disk IO caching stuff in disk.c? Or some
     * other weird bug? IDK?
     */
    device->f_write(device, gpt_buffer, total_required_size,
        device->effective_sector_size);
    device->f_write(device, gpt_buffer, 512, header_template->alt_lba);

    heap_free(gpt_buffer);

    return 0;
}

/*!
 * @brief: Check if @device is the disk we booted from
 *
 * How are we going to do that?
 * We'll need to ask our platform backend for the serial number of the
 * underlying disk or something We can do this in a few ways:
 *  - We can simply create a disk_dev_t function opt that backends can
 * implement, that give us access to the disks platform identifiers. This way we
 * can match the identifier of the boot device against the identifier of @device
 *  - We can have a hash field inside of disk_dev_t structs, which must be set
 * by the platform backend, when creating their disk devices. What exactly is
 * used inside the hash is up to the backend, but in the case of EFI, it will
 * most likely be the device path string or something.
 */
bool disk_did_boot_from(disk_dev_t* device)
{
    /* TODO: */
    return false;
}

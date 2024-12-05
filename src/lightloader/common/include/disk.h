#ifndef __LIGHTLOADER_DISK__
#define __LIGHTLOADER_DISK__

#include "fs.h"
#include "guid.h"
#include <stdint.h>

struct gpt_header;

#define DISK_FLAG_PARTITION 0x00000001
#define DISK_FLAG_OPTICAL 0x00000002
#define DISK_FLAG_REMOVABLE 0x00000004
#define DISK_FLAG_DID_INSTALL 0x00000008

#define DISK_FLAG_DATA_PART 0x00000010
#define DISK_FLAG_SYS_PART 0x00000020

/* Should always be directly to disk and not to our local cache first? */
#define DISK_FLAG_CACHE_WRITES 0x00000040

typedef struct disk_dev {

    uintptr_t first_sector;
    size_t total_size;
    size_t sector_size;
    size_t effective_sector_size;
    size_t total_sectors;
    uint32_t optimal_transfer_factor;
    uint32_t flags;

    /* Platform specific stuff */
    void* private;

    /*
     * We have 8 caches to accomedate 8 blocks of data on disk
     * when accessing a cache, we increase the usage count of that
     * cache by one. When we run out of caches, we clear the least
     * used cache. If there is no one cache that is used the least,
     * we clear the oldest cache
     */
    struct {
        uint8_t* cache_ptr[8];
        uint32_t cache_block[8];
        uint8_t cache_dirty_flags;
        uint8_t cache_oldest;
        uint32_t cache_size;
        uint8_t cache_usage_count[8];
    } cache;

    light_fs_t* filesystem;

    union {
        struct gpt_entry_t* partition_entry;
        struct gpt_header* partition_header;
    };

    struct disk_dev* next_partition;

    /* I/O opperations for every disk */
    int (*f_read)(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset);
    int (*f_write)(struct disk_dev* dev, void* buffer, size_t size, uintptr_t offset);
    int (*f_write_zero)(struct disk_dev* dev, size_t size, uintptr_t offset);
    int (*f_bread)(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba);
    int (*f_bwrite)(struct disk_dev* dev, void* buffer, size_t count, uintptr_t lba);
    int (*f_flush)(struct disk_dev* dev);
} disk_dev_t;

static inline void disk_enable_cache_writes(disk_dev_t* dev)
{
    dev->flags |= DISK_FLAG_CACHE_WRITES;
}

static inline void disk_disable_cache_writes(disk_dev_t* dev)
{
    dev->flags &= ~DISK_FLAG_CACHE_WRITES;
}

static inline bool disk_is_cache_writes(disk_dev_t* dev)
{
    return (dev->flags & DISK_FLAG_CACHE_WRITES) == DISK_FLAG_CACHE_WRITES;
}

void register_bootdevice(disk_dev_t* device);
void register_partition(disk_dev_t* device);

disk_dev_t* get_bootdevice();

void cache_gpt_entry(disk_dev_t* device);
void cache_gpt_header(disk_dev_t* device);

int disk_init_cache(disk_dev_t* device);

uint8_t disk_select_cache(disk_dev_t* device, uint64_t block);
int disk_clear_cache(disk_dev_t* device, uint64_t block);

int disk_flush(disk_dev_t* device);

int disk_install_partitions(disk_dev_t* device, bool add_gap);
bool disk_did_boot_from(disk_dev_t* device);

typedef struct gpt_header {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;

    uint32_t reserved;

    uint64_t my_lba;
    uint64_t alt_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;

    guid_t disk_guid;

    uint64_t partition_entry_lba;
    uint32_t partition_entry_num;
    uint32_t partition_entry_size;
    uint32_t partition_entry_array_crc32;
    // the rest of the block is reserved by UEFI and must be zero
} __attribute__((packed)) gpt_header_t;

/*
 * NOTE: Bits 48 to 63 are pretty much free to be used for whatever
 * we want, exept bit 60 to 63, since they are globally defined as 'standard'
 * GPT attributes, wich means that biosses and EFI firmwares might act
 * uppon these flags.
 */
#define GPT_ATTR_REQUIRED (1 << 0)
#define GPT_ATTR_EFI_IGNORE (1 << 1)
#define GPT_ATTR_BIOS_BOOTABLE (1 << 2)
#define GPT_ATTR_READONLY 0x1000000000000000ULL
#define GPT_ATTR_SHADOWCOPY 0x2000000000000000ULL
#define GPT_ATTR_HIDDEN 0x4000000000000000ULL
#define GPT_ATTR_NO_DRIVER_LETTER 0x8000000000000000ULL

typedef struct gpt_entry {

    guid_t partition_type_guid;
    guid_t unique_partition_guid;

    // range that the partition spans on disk, in blocks
    uint64_t starting_lba;
    uint64_t ending_lba;

    uint64_t attributes;

    // (UEFI specification, page 125)
    // Null-terminated string containing a
    // human-readable name of the partition.
    uint16_t partition_name[36];

} __attribute__((packed)) gpt_entry_t;

disk_dev_t* create_and_link_partition_dev(disk_dev_t* parent, uintptr_t start_lba, uintptr_t ending_lba);

#endif // !__LIGHTLOADER_DISK__

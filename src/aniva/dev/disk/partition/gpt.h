#ifndef __ANIVA_GPT_PARTITION__
#define __ANIVA_GPT_PARTITION__
#include "dev/disk/generic.h"
#include "libk/linkedlist.h"
#include <libk/hive.h>
#include <libk/stddef.h>

#define GPT_SIG_0 0x20494645
#define GPT_SIG_1 0x54524150

typedef struct gpt_partition {
  // TODO:
  uint8_t m_guid[16];
  char m_name[72];

  // Path how we can find this partition through 
  // Its parent device
  char* m_path;
  uintptr_t m_index;

  uint64_t m_start_lba;
  uint64_t m_end_lba;
} gpt_partition_t;

typedef struct gpt_partition_entry {
    uint8_t partition_guid[16];
    uint8_t unique_guid[16];

    uint64_t first_lba;
    uint64_t last_lba;

    uint64_t attributes;
    char partition_name[72];
} __attribute__((packed)) gpt_partition_entry_t;

typedef struct gpt_partition_header {
    uint32_t sig[2];
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32_header;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;

    uint64_t first_usable_lba;
    uint64_t last_usable_lba;

    uint64_t disk_guid1[2];

    uint64_t partition_array_start_lba;

    uint32_t entries_count;
    uint32_t partition_entry_size;
    uint32_t crc32_entries_array;
} __attribute__((packed)) gpt_partition_header_t;

typedef struct {
  gpt_partition_header_t m_header;

  generic_disk_dev_t* m_device;
  hive_t* m_partitions;
} gpt_table_t;

gpt_table_t* create_gpt_table(generic_disk_dev_t* device);
void destroy_gpt_table(gpt_table_t* table);

gpt_partition_t* create_gpt_partition(gpt_partition_entry_t* entry, uintptr_t index);
void destroy_gpt_partition(gpt_partition_t* partition);

#endif // !__ANIVA_GPT_PARTITION__
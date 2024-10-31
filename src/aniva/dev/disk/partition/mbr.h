#ifndef __ANIVA_PARTITION_MBR__
#define __ANIVA_PARTITION_MBR__

#include "libk/data/linkedlist.h"
#include <libk/stddef.h>

struct volume_device;

typedef struct {
    uint8_t status;
    uint8_t chs1[3];
    uint8_t type;
    uint8_t chs2[3];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed)) mbr_entry_t;

#define MBR_SIGNATURE 0xAA55

#define MBR_PROTECTIVE 0xEE

#define MBR_TYPE_EFI_SYS_PART 0xEF

#define EBR_CHS_CONTAINER 0x05
#define EBR_LBA_CONTAINER 0x0F

extern const char* mbr_partition_names[];

typedef union {
    struct {
        uint8_t bootstrap_code[446];
        mbr_entry_t entries[4];
        uint16_t signature;
    } legacy;
    struct {
        uint8_t bootstrap_code_1[218];
        uint16_t zero;
        uint8_t drive;
        uint8_t seconds;
        uint8_t minutes;
        uint8_t hours;
        uint8_t bootstrap_code_2[216];
        uint32_t disk_signature;
        uint16_t disk_signature_end;
        mbr_entry_t entries[4];
        uint16_t signature;
    } modern;
} __attribute__((packed)) mbr_header_t;

typedef struct mbr_table {
    mbr_header_t header_copy;
    uint8_t entry_count;

    list_t* partitions;
} mbr_table_t;

mbr_table_t* create_mbr_table(struct volume_device* device, uintptr_t start_lba);
void destroy_mbr_table(mbr_table_t* table);

#endif // !__ANIVA_PARTITION_MBR__

#ifndef __ANIVA_PARTITION_MBR__
#define __ANIVA_PARTITION_MBR__

struct disk_dev;

typedef struct {

} __attribute__((packed)) mbr_entry_t;

typedef struct {

} __attribute__((packed)) mbr_header_t;

typedef struct mbr_table {
  mbr_header_t header_copy;
  
} mbr_table_t;

mbr_table_t* create_mbr_table(struct disk_dev* device);
void destroy_mbr_table(mbr_table_t* table);

#endif // !__ANIVA_PARTITION_MBR__

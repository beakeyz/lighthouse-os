#ifndef __ANIVA_PARTITION_MBR__
#define __ANIVA_PARTITION_MBR__

struct disk_dev;

typedef struct {

} mbr_table_t;

void create_mbr_table(struct disk_dev* device);

#endif // !__ANIVA_PARTITION_MBR__

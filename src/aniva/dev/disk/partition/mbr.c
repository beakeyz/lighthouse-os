#include "mbr.h"
#include "libk/stddef.h"

mbr_table_t* create_mbr_table(struct disk_dev* device)
{
  (void)device;
  return nullptr;
}

void destroy_mbr_table(mbr_table_t* table)
{
  (void)table;
}

#include "mbr.h"
#include "dev/disk/generic.h"
#include "dev/disk/shared.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "mem/heap.h"

/*
 * Since there are most likely to only be a maximum of four mbr partitions, let's just 
 * hardcode most of em here right now lmao
 */
const char* mbr_partition_names[] = {
  "mbr_part0",
  "mbr_part1",
  "mbr_part2",
  "mbr_part3",
};

mbr_table_t* create_mbr_table(disk_dev_t* device, uintptr_t start_lba)
{
  int error;
  mbr_header_t* header;
  uint8_t buffer[device->m_logical_sector_size];
  mbr_table_t* table;

  if (device->m_logical_sector_size != 512)
    return nullptr;

  table = kmalloc(sizeof(mbr_table_t));

  /* When the sector size isn't right, we can't have mbr =/ */
  if (!table)
    return nullptr;

  /* The MBR should always be at offset 0 of a device */
  error = device->m_ops.f_read_blocks(device, buffer, 1, start_lba);

  if (error)
    goto dealloc_and_fail;

  header = (mbr_header_t*)buffer;

  /* FUCKKK, no signature =( */
  if (header->legacy.signature != MBR_SIGNATURE)
    goto dealloc_and_fail;

  table->partitions = init_list();
  table->entry_count = 0;

  for (uint32_t i = 0; i < 4; i++) {
    mbr_entry_t entry = header->legacy.entries[i];

    if (!entry.type)
      continue;

    table->entry_count++;

    /* Just append pointers to the raw mbr entries */
    list_append(table->partitions, &entry);
  }

  memcpy(&table->header_copy, header, sizeof(*header));

  return table;

dealloc_and_fail:
  kfree(table);
  return nullptr;
}

void destroy_mbr_table(mbr_table_t* table)
{
  destroy_list(table->partitions);

  kfree(table);
}

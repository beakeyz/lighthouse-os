#include "gpt.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/heap.h"
#include <libk/string.h>

static bool gpt_header_is_valid(gpt_partition_header_t* header) {

  if (header->sig[0] != GPT_SIG_0 || header->sig[1] != GPT_SIG_1)
    return false;

  return true;
}

static bool gpt_entry_is_used(uint8_t guid[16]) {

  // NOTE: a gpt entry is unused if all the GUID bytes are NULL
  for (uintptr_t i = 0; i < 16; i++) {
    if (guid[i] != 0x00) {
      return true;
    }
  }

  return false;
}

gpt_table_t* create_gpt_table(generic_disk_dev_t* device) {
  gpt_table_t* ret = kmalloc(sizeof(gpt_table_t));

  ret->m_device = device;
  ret->m_partitions = init_list();

  // TODO: get gpt header
  uint8_t buffer[device->m_logical_sector_size];
  uint32_t gpt_block = 0;

  if (device->m_logical_sector_size == 512)
    gpt_block = 512;

  int result = device->f_read_sync(device, buffer, device->m_logical_sector_size, gpt_block);

  if (result < 0) {
    goto fail_and_destroy;
  }

  memcpy(&ret->m_header, buffer, sizeof(gpt_partition_header_t));

  if (!gpt_header_is_valid(&ret->m_header)) {
    goto fail_and_destroy;
  }

  // Check partitions

  const size_t partition_entry_size = ret->m_header.partition_entry_size;
  disk_offset_t blk_offset = ret->m_header.partition_array_start_lba * device->m_logical_sector_size;

  for (uintptr_t i = 0; i < ret->m_header.entries_count; i++) {

    // Let's put the whole block into this buffer
    uint8_t entry_buffer[device->m_logical_sector_size];
    if (device->f_read_sync(device, entry_buffer, device->m_logical_sector_size, blk_offset) < 0) {
      goto fail_and_destroy;
    }

    gpt_partition_entry_t* entries = (gpt_partition_entry_t*)entry_buffer;

    gpt_partition_entry_t entry = entries[i % (device->m_logical_sector_size / partition_entry_size)];

    if (!gpt_entry_is_used(entry.partition_guid)) {
      blk_offset += partition_entry_size;
      continue;
    }

    list_append(ret->m_partitions, create_gpt_partition(&entry));

    blk_offset += partition_entry_size;
  }

  return ret;

fail_and_destroy:
  destroy_gpt_table(ret);
  return nullptr;
}

void destroy_gpt_table(gpt_table_t* table) {
  destroy_list(table->m_partitions);
  kfree(table);
}

gpt_partition_t* create_gpt_partition(gpt_partition_entry_t* entry) {
  gpt_partition_t* ret = kmalloc(sizeof(gpt_partition_t));

  memcpy(ret->m_guid, entry->partition_guid, 16);
  memcpy(ret->m_name, entry->partition_name, 72);
  ret->m_start_lba = entry->first_lba;
  ret->m_end_lba = entry->last_lba;

  return ret;
}

void destroy_gpt_partition(gpt_partition_t* partition) {
  kfree(partition);
}


#include "gpt.h"
#include "dev/disk/device.h"
#include "libk/data/linkedlist.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include <libk/string.h>

static char* gpt_partition_create_path(gpt_partition_t* partition)
{
    char* ret = kmalloc(16);

    memset(ret, 0, 16);

    if (KERR_ERR(sfmt(ret, "part%d", partition->m_index))) {
        kfree(ret);
        return nullptr;
    }

    return ret;
}

static bool gpt_header_is_valid(gpt_partition_header_t* header)
{
    if (header->sig[0] != GPT_SIG_0 || header->sig[1] != GPT_SIG_1)
        return false;

    return true;
}

static bool gpt_entry_is_used(uint8_t guid[16])
{

    // NOTE: a gpt entry is unused if all the GUID bytes are NULL
    for (uintptr_t i = 0; i < 16; i++) {
        if (guid[i] != 0x00) {
            return true;
        }
    }

    return false;
}

/*!
 * @brief Verify if the device has a valid GPT header and copy it if we have it
 *
 * Nothing to add here...
 */
bool disk_try_copy_gpt_header(volume_device_t* device, gpt_table_t* table)
{

    gpt_partition_header_t* header;
    uint8_t buffer[device->info.logical_sector_size];
    uint32_t gpt_block = 0;

    if (device->info.logical_sector_size == 512)
        gpt_block = 1;

    if (volume_dev_bread(device, gpt_block, buffer, 1) < 0)
        return false;

    header = (void*)buffer;

    if (!gpt_header_is_valid(header))
        return false;

    if (table)
        memcpy(&table->m_header, buffer, sizeof(gpt_partition_header_t));

    return true;
}

gpt_table_t* create_gpt_table(volume_device_t* device)
{
    gpt_table_t* ret;

    size_t partition_entry_size;
    uintptr_t partition_index;
    uintptr_t blk;
    uintptr_t offset;

    // Let's put the whole block into this buffer
    uint8_t entry_buffer[device->info.logical_sector_size];

    if (!device)
        return nullptr;

    ret = kmalloc(sizeof(gpt_table_t));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(gpt_table_t));

    ret->m_device = device;
    ret->m_partition_count = 0;
    ret->m_partitions = init_list();

    if (!disk_try_copy_gpt_header(device, ret))
        goto fail_and_destroy;

    // Check partitions
    partition_entry_size = ret->m_header.partition_entry_size;
    partition_index = 0;
    blk = ret->m_header.partition_array_start_lba;
    offset = blk * device->info.logical_sector_size;

    for (uintptr_t i = 0; i < ret->m_header.entries_count; i++) {

        if (volume_dev_bread(device, blk, entry_buffer, 1) < 0)
            goto fail_and_destroy;

        gpt_partition_entry_t* entries = (gpt_partition_entry_t*)entry_buffer;

        gpt_partition_entry_t entry = entries[i % (device->info.logical_sector_size / partition_entry_size)];

        if (!gpt_entry_is_used(entry.partition_guid))
            goto cycle_next;

        gpt_partition_t* partition = create_gpt_partition(&entry, partition_index);

        ret->m_partition_count++;
        list_append(ret->m_partitions, partition);

        partition_index++;
    cycle_next:
        offset += partition_entry_size;
        blk = (ALIGN_DOWN(offset, device->info.logical_sector_size) / device->info.logical_sector_size);
    }

    return ret;

fail_and_destroy:
    destroy_gpt_table(ret);
    return nullptr;
}

void destroy_gpt_table(gpt_table_t* table)
{

    FOREACH(i, table->m_partitions)
    {
        gpt_partition_t* partition = i->data;

        destroy_gpt_partition(partition);
    }

    destroy_list(table->m_partitions);
    kfree(table);
}

gpt_partition_t* create_gpt_partition(gpt_partition_entry_t* entry, uintptr_t index)
{
    gpt_partition_t* ret = kmalloc(sizeof(gpt_partition_t));

    memcpy(ret->m_type.m_guid, entry->partition_guid, 16);
    memcpy(ret->m_type.m_name, entry->partition_name, 72);

    gpt_part_type_decode_name(&ret->m_type);

    ret->m_start_lba = entry->first_lba;
    ret->m_end_lba = entry->last_lba;
    ret->m_index = index;
    ret->m_path = gpt_partition_create_path(ret);

    return ret;
}

void destroy_gpt_partition(gpt_partition_t* partition)
{
    kfree(partition->m_path);
    kfree(partition);
}

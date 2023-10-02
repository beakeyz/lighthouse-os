#include "generic.h"
#include "dev/debug/serial.h"
#include "dev/disk/partition/gpt.h"
#include "dev/disk/partition/mbr.h"
#include "fs/vfs.h"
#include "fs/vobj.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include <libk/string.h>
#include "libk/math/log2.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "ramdisk.h"
#include <sync/mutex.h>

//static char* s_root_dev_name;
//static char s_root_dev_name_buffer[64];
static mutex_t* s_gdisk_lock;

struct gdisk_store_entry {
  disk_dev_t* dev;
  struct gdisk_store_entry* next;
};

static struct gdisk_store_entry* s_gdisks;
static struct gdisk_store_entry* s_last_gdisk;

static disk_uid_t generate_new_disk_uid() {

  if (!mutex_is_locked_by_current_thread(s_gdisk_lock))
    return -1;

  struct gdisk_store_entry* entry = s_last_gdisk;

  disk_uid_t ret;

  if (!entry) {
    ret = 0;
    goto exit;
  }

  ret = entry->dev->m_uid + 1;

  // Label is here for eventual locking / allocation cleaning
exit:;

  return ret;
}

struct gdisk_store_entry** get_store_entry(disk_dev_t* device) {

  struct gdisk_store_entry** entry = &s_gdisks;

  for (; (*entry); entry = &(*entry)->next) {
    if ((*entry)->dev == device) {
      return entry;
    }
  }

  return entry;
}

static struct gdisk_store_entry* create_gdisk_store_entry(disk_dev_t* device) {
  struct gdisk_store_entry* ret = kmalloc(sizeof(struct gdisk_store_entry));

  memset(ret, 0, sizeof(struct gdisk_store_entry));

  ret->dev = device;
  ret->next = nullptr;

  device->m_uid = generate_new_disk_uid();

  return ret;
}

static void destroy_gdisk_store_entry(struct gdisk_store_entry* entry) {

  if (!entry)
    return;

  if (entry->dev)
    entry->dev->m_uid = (disk_uid_t)-1;

  kfree(entry);
}

partitioned_disk_dev_t* create_partitioned_disk_dev(disk_dev_t* parent, char* name, uint64_t start, uint64_t end, uint32_t flags) {
  partitioned_disk_dev_t* ret = kmalloc(sizeof(partitioned_disk_dev_t));
  
  ret->m_parent = parent;
  ret->m_name = name;

  /* FIXME: Should we align these? */
  ret->m_start_lba = start;
  ret->m_end_lba = end;

  /* TODO: check for range */
  ret->m_block_size = parent->m_logical_sector_size;

  ret->m_flags = flags;

  ret->m_next = nullptr;

  return ret;
}

void destroy_partitioned_disk_dev(partitioned_disk_dev_t* dev) {
  kfree(dev);
}

void attach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev) {

  partitioned_disk_dev_t** device;
  
  /*
   * This code is stupid
   */
  for (device = &generic->m_devs; *device; device = &(*device)->m_next) {
    if (!(*device)) {
      break;
    }
  }

  if (!(*device)) {
    *device = dev;
    generic->m_partitioned_dev_count++;
  }
}

void detach_partitioned_disk_device(disk_dev_t* generic, partitioned_disk_dev_t* dev) {

  if (!dev)
    return;

  partitioned_disk_dev_t** previous = nullptr;
  partitioned_disk_dev_t** device;

  for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
    if ((*device) == dev) {
      if (previous) {
        (*previous)->m_next = (*device)->m_next;
      }
      break;
    }

    previous = device;
  }

  ASSERT_MSG(generic->m_partitioned_dev_count, "Tried to detach partitioned disk device when the dev count of the generic disk device was already zero!");

  generic->m_partitioned_dev_count--;
}

partitioned_disk_dev_t** fetch_designated_device_for_block(disk_dev_t* generic, uintptr_t block) {

  partitioned_disk_dev_t** device;
  
  for (device = &generic->m_devs; (*device); device = &(*device)->m_next) {
    uintptr_t start_lba = (*device)->m_start_lba;
    uintptr_t end_lba = (*device)->m_end_lba;

    // Check if the block is contained in this partition
    if (block >= start_lba && block <= end_lba) {
      return device;
    }
  }

  return nullptr;
}

int generic_disk_opperation(disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {
  return -1;
}

ErrorOrPtr register_gdisk_dev(disk_dev_t* device) {
  struct gdisk_store_entry** entry = get_store_entry(device);

  /* We need to assert that get_store_entry returned an empty entry */
  if (*entry) {
    return Error();
  }

  mutex_lock(s_gdisk_lock);

  *entry = create_gdisk_store_entry(device);

  /* Then update the tail pointer */
  s_last_gdisk = *entry;

  mutex_unlock(s_gdisk_lock);

  return Success(0);
}

/* TODO: locking */
ErrorOrPtr unregister_gdisk_dev(disk_dev_t* device) {
  struct gdisk_store_entry** entry = &s_gdisks;
  struct gdisk_store_entry** previous_entry = nullptr;
  
  /* First we detacht the entry */
  for (; *entry; entry = &(*entry)->next) {
    /* Found the right entry */
    if ((*entry)->dev == device) {
      if (previous_entry && *previous_entry) {
        /* Detach */
        (*previous_entry)->next = (*entry)->next;

        /* Fixup the pointer to the last entry if we remove that entry */
        if (!(*entry)->next) {
          s_last_gdisk = *previous_entry;
        }
      } else {
        /* This is the first entry */
        s_gdisks = (*entry)->next;
      }
      break;
    }

    previous_entry = entry;
  }

  /* No entry found =/ */
  if (!*entry)
    return Error();

  /* If we remove the only entry, update s_last_gdisk */
  if ((!previous_entry || !*previous_entry) && !(*entry)->next && s_last_gdisk)
    s_last_gdisk = nullptr;

  struct gdisk_store_entry* current = (*entry)->next;

  /* Fixup the uids of the devices after this one */
  while (current) {
    current->dev->m_uid--;

    current = current->next;
  }

  /* Object not needed anymore, destroy it */
  destroy_gdisk_store_entry(*entry);

  return Success(0);
}

disk_dev_t* find_gdisk_device(disk_uid_t uid) {

  disk_uid_t i = 0; 

  for (struct gdisk_store_entry* entry = s_gdisks; entry; entry = entry->next) {
    if (i == uid) {
      if (entry->dev->m_uid == uid)
        return entry->dev;

      /* Shit, something didn't match */
      break;
    }
    i++;
  }

  /* FIXME: what can we do to fix the indexing issues at this point? */

  return nullptr;
}

/* TODO: locking */
ErrorOrPtr register_gdisk_dev_with_uid(disk_dev_t* device, disk_uid_t uid) {

  if (uid == 0) {
    struct gdisk_store_entry* entry = create_gdisk_store_entry(device);
    /* Match the uid assignment in the create call */
    entry->dev->m_uid = uid;

    /* Place at the front */
    entry->next = s_gdisks;
    s_gdisks = entry;

    entry = s_gdisks->next;

    ASSERT_MSG(entry->dev->m_uid == 0, "gdisk store seems unsorted! (uid of first entry != 0)");

    while (entry) {
      entry->dev->m_uid++;
      entry = entry->next;
    }
  } else {

    struct gdisk_store_entry* entry = s_gdisks;
    struct gdisk_store_entry* previous = nullptr;

    for (; entry; entry = entry->next) {

      /* Found the entry we need to 'shift forward' */
      if (entry->dev->m_uid == uid)
        break;
      
      previous = entry;
    }

    struct gdisk_store_entry* new_entry = create_gdisk_store_entry(device);
    new_entry->dev->m_uid = uid;

    /* Emplace */
    previous->next = new_entry;
    new_entry->next = entry;

    /* Fixup the uids */
    while (entry) {

      entry->dev->m_uid++;

      entry = entry->next;
    }
  }

  return Success(0);
}

int read_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {

  int result = -1;

  if (!dev || !dev->m_parent)
    return result;


  const uintptr_t block = offset / dev->m_parent->m_logical_sector_size;

  if (block >= dev->m_start_lba && block <= dev->m_end_lba) {

    if (dev->m_parent->m_ops.f_read_sync)
      result = dev->m_parent->m_ops.f_read_sync(dev->m_parent, buffer, size, offset);

  }

  return result;
}

int write_sync_partitioned(partitioned_disk_dev_t* dev, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement write_sync_partitioned");
  return 0;
}

int read_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {

  if (!dev || !dev->m_parent)
    return -1;

  int result = -1;
  disk_offset_t offset;

  block = dev->m_start_lba + block;

  if (block <= dev->m_end_lba) {

    offset = block * dev->m_parent->m_logical_sector_size;

    if (dev->m_parent->m_ops.f_read_sync) {
      result = dev->m_parent->m_ops.f_read_sync(dev->m_parent, buffer, size, offset);
    } else {
      // Do we have an alternative?
    }
  }

  return result;
}

int write_sync_partitioned_blocks(partitioned_disk_dev_t* dev, void* buffer, size_t size, uintptr_t block) {
  kernel_panic("TODO: implement write_sync_partitioned_block");
}

int pd_bread(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn) 
{
  if ((dev->m_flags & PD_FLAG_ONLY_SYNC) == PD_FLAG_ONLY_SYNC)
    return read_sync_partitioned_blocks(dev, buffer, dev->m_block_size, blockn);

  kernel_panic("TODO: implement async disk IO");
}

int pd_bwrite(partitioned_disk_dev_t* dev, void* buffer, uintptr_t blockn) 
{
  if ((dev->m_flags & PD_FLAG_ONLY_SYNC) == PD_FLAG_ONLY_SYNC)
    return write_sync_partitioned_blocks(dev, buffer, dev->m_block_size, blockn);

  kernel_panic("TODO: implement async disk IO");
}

int pd_set_blocksize(partitioned_disk_dev_t* dev, uint32_t blksize)
{
  if (blksize > SMALL_PAGE_SIZE || blksize < 512 || !POWER_OF_2(blksize))
    return -1;

  if (blksize < dev->m_parent->m_logical_sector_size)
    return -1;

  dev->m_block_size = blksize;

  return 0;
}

disk_dev_t* create_generic_ramdev(size_t size) {

  size = ALIGN_UP(size, SMALL_PAGE_SIZE);
  uintptr_t start_addr = Must(__kmem_kernel_alloc_range(size, KMEM_CUSTOMFLAG_CREATE_USER, 0));

  disk_dev_t* dev = create_generic_ramdev_at(start_addr, size);

  return dev;
}

/*
 * We leave it to the caller to map the addressspace
 * TODO: maybe we could pass an addressspace object that
 * enforces mappings...
 */
disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size) {
  /* Trolle xD */
  const size_t ramdisk_blksize = sizeof(uint8_t);

  disk_dev_t* dev = kmalloc(sizeof(disk_dev_t));
  memset(dev, 0x00, sizeof(disk_dev_t));

  dev->m_flags |= GDISKDEV_FLAG_RAM;
  dev->m_parent = nullptr;
  dev->m_ops.f_read_sync = ramdisk_read;
  dev->m_ops.f_write_sync = ramdisk_write;
  dev->m_logical_sector_size = ramdisk_blksize;
  dev->m_physical_sector_size = ramdisk_blksize;
  dev->m_device_name = "ramdev"; /* Should we create a unique name? */
  dev->m_max_blk = size;

  /* A ramdisk can't use async IO by definition */
  partitioned_disk_dev_t* partdev = create_partitioned_disk_dev(dev, "rampart0", address, address + size, PD_FLAG_ONLY_SYNC);
  
  attach_partitioned_disk_device(dev, partdev);

  return dev;

}

bool destroy_generic_ramdev(disk_dev_t* device) {

  /* If we get passed a faulty ramdevice, just die */
  if (!device || (device->m_flags & GDISKDEV_FLAG_RAM) == 0 || !device->m_devs || device->m_devs->m_next)
    return false;

  const uintptr_t start_addr = device->m_devs->m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_end_lba;

  const size_t ramdev_size = end_addr - start_addr;

  ErrorOrPtr result = __kmem_kernel_dealloc(start_addr, ramdev_size);

  if (result.m_status != ANIVA_SUCCESS)
    return false;

  /* We should only have one partition, so this works */
  destroy_partitioned_disk_dev(device->m_devs);

  kfree(device);

  return true;
}

partitioned_disk_dev_t* find_partition_at(disk_dev_t* diskdev, uint32_t idx) {

  partitioned_disk_dev_t* ret;

  if (!diskdev || idx >= diskdev->m_partitioned_dev_count)
    return nullptr;

  ret = diskdev->m_devs;

  /* TODO: find out if this is faster than normal linear scan */
  while (idx) {
    if (!ret)
      return nullptr;

    if (idx % 2 == 0) {
      ret = ret->m_next->m_next;
      idx -= 2;
    } else {
      ret = ret->m_next;
      idx--;
    }
  }

  return ret;
}

static int __diskdev_populate_mbr_part_table(disk_dev_t* dev)
{
  mbr_table_t* mbr_table = create_mbr_table(dev, 0);

  /* No valid mbr prob, yikes */
  if (!mbr_table)
    return -1;

  dev->mbr_table = mbr_table;
  dev->m_partition_type = PART_TYPE_MBR; 
  dev->m_partitioned_dev_count = 0;

  FOREACH(i, mbr_table->partitions) {
    mbr_entry_t* entry = i->data;

    partitioned_disk_dev_t* partitioned_device = create_partitioned_disk_dev(dev, (char*)mbr_partition_names[dev->m_partitioned_dev_count], entry->offset, (entry->offset + entry->length) - 1, PD_FLAG_ONLY_SYNC);

    attach_partitioned_disk_device(dev, partitioned_device);
  }

  return 0;
}

static int __diskdev_populate_gpt_part_table(disk_dev_t* dev)
{
  /* Lay out partitions (FIXME: should we really do this here?) */
  gpt_table_t* gpt_table = create_gpt_table(dev);

  if (!gpt_table)
    return -1;

  /* Cache the gpt table */
  dev->gpt_table = gpt_table;
  dev->m_partition_type = PART_TYPE_GPT;
  dev->m_partitioned_dev_count = 0;

  /*
   * TODO ?: should we put every partitioned device in the vfs?
   * Neh, that whould create the need to implement file opperations
   * on devices
   */

  /* Attatch the partition devices to the generic disk device */
  FOREACH(i, gpt_table->m_partitions) {
    gpt_partition_t* part = i->data;

    partitioned_disk_dev_t* partitioned_device = create_partitioned_disk_dev(dev, part->m_type.m_name, part->m_start_lba, part->m_end_lba, PD_FLAG_ONLY_SYNC);

    attach_partitioned_disk_device(dev, partitioned_device);
  }
  return 0;
}

/*!
 * @brief Grab the correct partitioning sceme and populate the partitioned device link
 *
 * Nothing to add here...
 */
int diskdev_populate_partition_table(disk_dev_t* dev)
{
  int error;

  /* Just in case */
  dev->m_partition_type = PART_TYPE_NONE;

  error = __diskdev_populate_gpt_part_table(dev);

  /* On an error, go next */
  if (!error)
    return 0;

  error = __diskdev_populate_mbr_part_table(dev);

  if (!error)
    return 0;

  return -1;
}

int ramdisk_read(disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {

  if (!device || !device->m_devs || !device->m_physical_sector_size)
    return -1;

  if (!buffer || !size)
    return -1;

  /* A ramdisk device always only contains one 'partition' */
  if (device->m_devs->m_next)
    return -1;

  const uintptr_t start_addr = device->m_devs->m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_end_lba;

  const uintptr_t read_addr = start_addr + offset;

  if (read_addr > end_addr || read_addr + size > end_addr)
    return -1;

  memcpy(buffer, (void*)read_addr, size);

  return 0;
}

int ramdisk_write(disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement ramdisk_write");
}

/*!
 * @brief Deallocate any structures or caches owned by the disk device
 *
 * Nothing to add here...
 */
void destroy_generic_disk(disk_dev_t* device)
{
  partitioned_disk_dev_t* itter;
  partitioned_disk_dev_t* current;
  
  switch (device->m_partition_type) {
    case PART_TYPE_GPT:
      destroy_gpt_table(device->gpt_table);
      break;
    case PART_TYPE_MBR:
      //destroy_mbr_table(device->mbr_table);
      break;
    case PART_TYPE_NONE:
      break;
  }

  itter = device->m_devs;

  while (itter) {
    /* Cache next before we destroy it */
    current = itter;
    itter = itter->m_next;

    destroy_partitioned_disk_dev(current);
  }

}

void register_boot_device() {
  /* We need some concrete way to detirmine boot device */
  kernel_panic("TODO: implement register_boot_device");
}

void init_gdisk_dev() {
  s_gdisk_lock = create_mutex(0);
}

static bool try_mount_root(partitioned_disk_dev_t* device)
{
  ErrorOrPtr result;
  vobj_t* scan_obj;
  const char* filesystems[] = {
    "fat32",
    "ext2",
  };
  const uint32_t filesystems_count = sizeof(filesystems) / sizeof(*filesystems);

  for (uint32_t i = 0; i < filesystems_count; i++) {
    const char* fs = filesystems[i];

    result = vfs_mount_fs(VFS_ROOT, VFS_DEFAULT_ROOT_MP, fs, device);

    /* Successful mount? try and find aniva.elf */
    if (!IsError(result)) {
      scan_obj = vfs_resolve(VFS_DEFAULT_ROOT_MP"/aniva.elf");

      if (!scan_obj) {
        result = Error();

        vfs_unmount(VFS_DEFAULT_ROOT_MP);
        continue;
      }

      /*
       * We could find the system file! 
       * TODO: look for more files that only one O.o
       */
      vobj_close(scan_obj);
      break;
    }
  }

  /* Failed to scan for filesystem */
  if (IsError(result))
    return false;

  return true;
}

void init_root_device_probing() {

  /*
   * Init probing for the root device
   */
  disk_dev_t* root_device;
  disk_uid_t device_index;

  partitioned_disk_dev_t* root_ramdisk;

  device_index = 0;
  root_ramdisk = nullptr;
  root_device = find_gdisk_device(0);

  while (root_device) {

    partitioned_disk_dev_t* part = root_device->m_devs;

    if (root_device->m_flags & GDISKDEV_FLAG_RAM) {

      /* We can use this ramdisk as a fallback to mount */
      root_ramdisk = part; 
      goto cycle_next;
    }

    /*
     * NOTE: we use this as a last resort. If there is no mention of a root device anywhere,
     * we bruteforce every partition and check if it contains a valid FAT32 filesystem. If
     * it does, we check if it contains the files that make up our boot filesystem and if it does, we simply take 
     * that device and mark as our root.
     */
    while (part) {

      /* Try to mount a filesystem and scan for aniva system files */
      if (try_mount_root(part))
        break;

      part = part->m_next;
    }

cycle_next:
    device_index++;
    root_device = find_gdisk_device(device_index);
  }

  if (!root_device) {
      if (!root_ramdisk || IsError(vfs_mount_fs(VFS_ROOT, VFS_DEFAULT_ROOT_MP, "cramfs", root_ramdisk))) {
        kernel_panic("Could not find a root device to mount! TODO: fix");
      }
  }

  kernel_panic("init_root_device_probing test");
}


bool gdisk_is_valid(disk_dev_t* device)
{
  return (
      device &&
      device->m_devs &&
      device->m_path &&
      (device->m_ops.f_read || device->m_ops.f_read_sync) &&
      device->m_max_blk
      );
}

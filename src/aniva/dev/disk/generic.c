#include "generic.h"
#include "dev/debug/serial.h"
#include "fs/vfs.h"
#include "libk/error.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "ramdisk.h"
#include <sync/mutex.h>

static char* s_root_dev_name;
static char s_root_dev_name_buffer[64];
static mutex_t* s_gdisk_lock;

struct gdisk_store_entry {
  generic_disk_dev_t* dev;
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

struct gdisk_store_entry** get_store_entry(generic_disk_dev_t* device) {

  struct gdisk_store_entry** entry = &s_gdisks;

  for (; (*entry); entry = &(*entry)->next) {
    if ((*entry)->dev == device) {
      return entry;
    }
  }

  return entry;
}

static struct gdisk_store_entry* create_gdisk_store_entry(generic_disk_dev_t* device) {
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

ErrorOrPtr register_gdisk_dev(generic_disk_dev_t* device) {
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
ErrorOrPtr unregister_gdisk_dev(generic_disk_dev_t* device) {
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

generic_disk_dev_t* find_gdisk_device(disk_uid_t uid) {

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
ErrorOrPtr register_gdisk_dev_with_uid(generic_disk_dev_t* device, disk_uid_t uid) {

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


  const uintptr_t block = offset * dev->m_parent->m_logical_sector_size;

  if (block >= dev->m_partition_data.m_start_lba && block <= dev->m_partition_data.m_end_lba) {

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

  block = dev->m_partition_data.m_start_lba + block;

  if (block <= dev->m_partition_data.m_end_lba) {

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

generic_disk_dev_t* create_generic_ramdev(size_t size) {

  size = ALIGN_UP(size, SMALL_PAGE_SIZE);
  uintptr_t start_addr = Must(__kmem_kernel_alloc_range(size, KMEM_CUSTOMFLAG_CREATE_USER, 0));

  generic_disk_dev_t* dev = create_generic_ramdev_at(start_addr, size);

  return dev;
}

/*
 * We leave it to the called to map the addressspace
 * TODO: maybe we could pass an addressspace object that
 * enforces mappings...
 */
generic_disk_dev_t* create_generic_ramdev_at(uintptr_t address, size_t size) {
  /* Trolle xD */
  const size_t ramdisk_blksize = 1;
  paddr_t start_addr = address;

  generic_disk_dev_t* dev = kmalloc(sizeof(generic_disk_dev_t));
  memset(dev, 0x00, sizeof(generic_disk_dev_t));

  dev->m_flags |= GDISKDEV_RAM;
  dev->m_parent = nullptr;
  dev->m_ops.f_read_sync = ramdisk_read;
  dev->m_ops.f_write_sync = ramdisk_write;
  dev->m_logical_sector_size = ramdisk_blksize;
  dev->m_physical_sector_size = ramdisk_blksize;
  dev->m_device_name = "ramdev"; /* Should we create a unique name? */
  dev->m_max_blk = size;

  generic_partition_t part = {
    .m_start_lba = start_addr,
    .m_end_lba = start_addr + size,
    .m_name = "rampart0",
  };

  partitioned_disk_dev_t* partdev = create_partitioned_disk_dev(dev, part);
  
  attach_partitioned_disk_device(dev, partdev);

  return dev;

}

bool destroy_generic_ramdev(generic_disk_dev_t* device) {

  /* If we get passed a faulty ramdevice, just die */
  if (!device || (device->m_flags & GDISKDEV_RAM) == 0 || !device->m_devs || device->m_devs->m_next)
    return false;

  const uintptr_t start_addr = device->m_devs->m_partition_data.m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_partition_data.m_end_lba;

  const size_t ramdev_size = end_addr - start_addr;

  ErrorOrPtr result = __kmem_kernel_dealloc(start_addr, ramdev_size);

  if (result.m_status != ANIVA_SUCCESS)
    return false;

  /* We should only have one partition, so this works */
  destroy_partitioned_disk_dev(device->m_devs);

  kfree(device);

  return true;
}

int ramdisk_read(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {

  if (!device || !device->m_devs || !device->m_physical_sector_size)
    return -1;

  if (!buffer || !size)
    return -1;

  /* A ramdisk device always only contains one 'partition' */
  if (device->m_devs->m_next)
    return -1;

  const uintptr_t start_addr = device->m_devs->m_partition_data.m_start_lba;
  const uintptr_t end_addr = device->m_devs->m_partition_data.m_end_lba;

  const uintptr_t read_addr = start_addr + offset;

  if (read_addr > end_addr || read_addr + size > end_addr)
    return -1;

  memcpy(buffer, (void*)read_addr, size);

  return 0;
}

int ramdisk_write(generic_disk_dev_t* device, void* buffer, size_t size, disk_offset_t offset) {
  kernel_panic("TODO: implement ramdisk_write");
}

void register_boot_device() {
  /* We need some concrete way to detirmine boot device */
  kernel_panic("TODO: implement register_boot_device");
}

void init_gdisk_dev() {
  s_gdisk_lock = create_mutex(0);

}

void init_root_device_probing() {

  /*
   * Init probing for the root device
   */
  generic_disk_dev_t* root_device = find_gdisk_device(0);

  if (!root_device) {
    /* if this does not work, there might just not be any harddisks connected, we should check for usb or ramdisks.. */
    kernel_panic("Could not find root disk. No disks atached?");
  }

  partitioned_disk_dev_t* part = root_device->m_devs;

  /*
   * NOTE: we use this as a last resort. If there is no mention of a root device anywhere,
   * we bruteforce every partition and check if it contains a valid FAT32 filesystem. If
   * it does, we check if it contains a kconfig file and if it does, we simply take 
   * that drive and partition as our root.
   */
  while (part) {

    print("Testing for fat filesystem: ");
    println(part->m_partition_data.m_name);

    /*
     * Test for a fat32 filesystem
     */
    if (vfs_mount_fs("/root", "fat32", part).m_status == ANIVA_SUCCESS)
      break;
    
    part = part->m_next;
  }

  kernel_panic("init_root_device_probing test");

}


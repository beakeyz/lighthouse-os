#include "ahci_port.h"
#include "ahci_device.h"
#include "dev/device.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/generic.h"
#include "dev/disk/shared.h"
#include "dev/endpoint.h"
#include "dev/pci/definitions.h"
#include "devices/shared.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"
#include <mem/heap.h>

static int ahci_port_read(device_t* port, void* buffer, disk_offset_t offset, size_t size);
static int ahci_port_write(device_t* port, void* buffer, disk_offset_t offset, size_t size);
static int ahci_port_flush(device_t* device);

static int ahci_port_write_blk(device_t* device, void* buffer, uintptr_t blk, size_t count);
static int ahci_port_read_blk(device_t* device, void* buffer, uintptr_t blk, size_t count);

static void decode_disk_model_number(char* model_number)
{
    for (uintptr_t chunk = 0; chunk < 40; chunk += 2) {
        char first_char = model_number[chunk];

        if (!first_char)
            return;

        model_number[chunk] = model_number[chunk + 1];
        model_number[chunk + 1] = first_char;
    }

    // Let's shave off trailing spaces
    char prev = NULL;

    for (uintptr_t i = 0; i < 40; i++) {

        // After we've found 2 spaces in a row, let's
        // assume that we have found the end of the modelnumber
        if (model_number[i] == ' ' && prev == ' ' && i > 1) {
            // Nullterminate
            model_number[i] = '\0';
            continue;
        }

        prev = model_number[i];
    }
}

static ALWAYS_INLINE uint32_t ahci_port_mmio_read32(ahci_port_t* port, uintptr_t offset)
{
    volatile uint32_t* data = (volatile uint32_t*)(port->m_port_offset + offset);
    return *data;
}

static ALWAYS_INLINE void ahci_port_mmio_write32(ahci_port_t* port, uintptr_t offset, uint32_t data)
{
    volatile uint32_t* current_data = (volatile uint32_t*)(port->m_port_offset + offset);
    *current_data = data;
}

static ALWAYS_INLINE uint32_t ahci_port_find_free_cmd_header(ahci_port_t* port)
{
    uint32_t ci = ahci_port_mmio_read32(port, AHCI_REG_PxCI);
    for (uint32_t idx = 0; idx < 32; idx++) {
        if ((ci & 1) == 0) {
            return idx;
        }
        ci >>= 1;
    }
    return (uint32_t)-1;
}

static ALWAYS_INLINE ANIVA_STATUS ahci_port_await_dma_completion(ahci_port_t* port)
{
    if (port->m_is_waiting) {

        while (port->m_awaiting_dma_transfer_complete) {
            scheduler_yield();
        }

        // Reset
        port->m_is_waiting = false;
        if (port->m_transfer_failed) {
            port->m_transfer_failed = false;
            // TODO: real dbg msg
            println("AHCIPORT: dma transfer failed!");
            return ANIVA_FAIL;
        }
    }
    return ANIVA_SUCCESS;
}

static ALWAYS_INLINE ANIVA_STATUS ahci_port_await_dma_completion_sync(ahci_port_t* port)
{

    if (port->m_is_waiting) {

        uintptr_t tmt;

        tmt = 0;

        // Wait until CI resets
        while (ahci_port_mmio_read32(port, AHCI_REG_PxCI)) {
            tmt++;

            // Max. 1 second
            if (tmt >= 1000) {
                goto timeout;
            }

            udelay(1000);
        }

        port->m_is_waiting = false;
        // TODO: check error
        return ANIVA_SUCCESS;
    }

timeout:
    return ANIVA_FAIL;
}

static ANIVA_STATUS ahci_port_send_command(ahci_port_t* port, uint8_t cmd, paddr_t buffer, size_t size, bool write, uintptr_t lba, uint16_t blk_count)
{

    if (ahci_port_await_dma_completion(port) == false)
        return ANIVA_FAIL;

    uint32_t cmd_header_idx = ahci_port_find_free_cmd_header(port);

    ASSERT_MSG(cmd_header_idx >= 0 && cmd_header_idx <= 32, "Invalid Command AHCI header!");

    CommandHeader_t* cmd_header = &((CommandHeader_t*)port->m_cmd_list_page)[cmd_header_idx];
    CommandTable_t* cmd_table = &((CommandTable_t*)port->m_cmd_table_buffer)[cmd_header_idx];

    cmd_header->command_table_base_addr = (uintptr_t)(kmem_to_phys(nullptr, (vaddr_t)cmd_table)) & 0xFFFFFFFFUL;
    cmd_header->command_table_base_addr_upper = 0;
    cmd_header->phys_region_desc_table_lenght = size ? 1 : 0;
    cmd_header->phys_region_desc_table_byte_count = 0;

    cmd_header->attr = 5 | (1 << 7);

    /*
    if (atapi) {
      cmd_header->attr |= (1 << 5);
    }
    */

    if (write) {
        cmd_header->attr |= (1 << 6);
    }

    // TODO: scatter list
    if (size > 0) {
        PhysRegionDesc* desc = &cmd_table->descriptors;
        desc->base_low = buffer;
        desc->base_high = 0;
        desc->reserved0 = 0;
        desc->byte_count = (size - 1) | (1 << 31);
    }

    CommandFIS_t* cmd_fis = &cmd_table->fis;

    memset(cmd_fis, 0, 64);

    cmd_fis->type = AHCI_FIS_TYPE_REG_H2D;
    cmd_fis->flags = 0x80;
    cmd_fis->command = cmd;
    cmd_fis->lba0 = lba & 0xFF;
    cmd_fis->lba1 = (lba >> 8) & 0xFF;
    cmd_fis->lba2 = (lba >> 16) & 0xFF;
    cmd_fis->lba3 = (lba >> 24) & 0xFF;
    cmd_fis->lba4 = (lba >> 32) & 0xFF;
    cmd_fis->lba5 = (lba >> 40) & 0xFF;
    cmd_fis->count = blk_count;
    cmd_fis->dev = 0x40;

    port->m_is_waiting = true;
    port->m_awaiting_dma_transfer_complete = true;
    port->m_transfer_failed = false;

    uint32_t ci = ahci_port_mmio_read32(port, AHCI_REG_PxCI);
    ci |= (1 << cmd_header_idx);
    ahci_port_mmio_write32(port, AHCI_REG_PxCI, ci);

    // FIXME: the write above here does not get accepted?
    // Qemu trace seems to show AHCI does do something with this,
    // but there just is no response in the form of an interrupt...
    return ANIVA_SUCCESS;
}

static ALWAYS_INLINE void port_spinup(ahci_port_t* port);
static ALWAYS_INLINE void port_power_on(ahci_port_t* port);
static ALWAYS_INLINE void port_start_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_set_active(ahci_port_t* port);
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port);
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port);

static int ahci_get_devinfo(device_t* device, DEVINFO* binfo)
{
    disk_dev_t* diskdev;

    memset(binfo, 0, sizeof(*binfo));

    diskdev = device->private;

    if (!diskdev)
        return -1;

    sfmt((char*)binfo->devicename, "%s", diskdev->m_device_name);

    binfo->ctype = DEVICE_CTYPE_AHCI;
    binfo->class = MASS_STORAGE;
    binfo->subclass = PCI_SUBCLASS_SATA;

    return 0;
}

static struct device_generic_endpoint _ahci_gen_ep = {
    .f_read = ahci_port_read,
    .f_write = ahci_port_write,
    .f_get_devinfo = ahci_get_devinfo,
};

static struct device_disk_endpoint _ahci_disk_ep = {
    .f_bread = ahci_port_read_blk,
    .f_bwrite = ahci_port_write_blk,
    .f_flush = ahci_port_flush,
};

static device_ep_t _ahci_eps[] = {
    DEVICE_ENDPOINT(ENDPOINT_TYPE_GENERIC, _ahci_gen_ep),
    DEVICE_ENDPOINT(ENDPOINT_TYPE_DISK, _ahci_disk_ep),
    {
        NULL,
    },
};

ahci_port_t* create_ahci_port(struct ahci_device* device, uintptr_t port_offset, uint32_t index)
{
    ahci_port_t* ret = kmalloc(sizeof(ahci_port_t));
    ret->m_port_index = index;
    ret->m_device = device;
    ret->m_port_offset = port_offset;

    ASSERT(!kmem_prepare_new_physical_page(&ret->m_ib_page));

    ret->m_hard_lock = create_spinlock(NULL);

    /* Flags? */
    ret->m_awaiting_dma_transfer_complete = false;
    ret->m_transfer_failed = false;
    ret->m_is_waiting = false;

    ret->m_generic = create_generic_disk(device->m_parent, NULL, ret, _ahci_eps);

    // prepare buffers
    ASSERT(!__kmem_kernel_alloc_range((void**)&ret->m_fis_recieve_page, SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA));
    ASSERT(!__kmem_kernel_alloc_range((void**)&ret->m_cmd_list_page, SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA));

    ASSERT(!__kmem_kernel_alloc_range((void**)&ret->m_dma_buffer, SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA));
    ASSERT(!__kmem_kernel_alloc_range((void**)&ret->m_cmd_table_buffer, SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA));

    return ret;
}

void destroy_ahci_port(ahci_port_t* port)
{

    __kmem_kernel_dealloc(port->m_dma_buffer, SMALL_PAGE_SIZE);
    __kmem_kernel_dealloc(port->m_cmd_list_page, SMALL_PAGE_SIZE);
    __kmem_kernel_dealloc(port->m_fis_recieve_page, SMALL_PAGE_SIZE);
    __kmem_kernel_dealloc(port->m_cmd_table_buffer, SMALL_PAGE_SIZE);

    destroy_spinlock(port->m_hard_lock);

    destroy_generic_disk(port->m_generic);

    kfree(port);
}

ANIVA_STATUS initialize_port(ahci_port_t* port)
{

    if (!port_has_phy(port)) {
        return ANIVA_FAIL;
    }

    // Reset errors
    ahci_port_mmio_write32(port, AHCI_REG_PxSERR, ahci_port_mmio_read32(port, AHCI_REG_PxSERR));

    port_set_sleeping(port);

    memset((void*)port->m_fis_recieve_page, 0, SMALL_PAGE_SIZE);
    memset((void*)port->m_cmd_list_page, 0, SMALL_PAGE_SIZE);

    paddr_t cmd_list = kmem_to_phys(nullptr, port->m_cmd_list_page);
    paddr_t fis_recieve = kmem_to_phys(nullptr, port->m_fis_recieve_page);

    // TODO: check for CAP.S64A so we can set CLBU and FBU accortingly
    ahci_port_mmio_write32(port, AHCI_REG_PxCLB, cmd_list);
    ahci_port_mmio_write32(port, AHCI_REG_PxCLBU, 0);
    ahci_port_mmio_write32(port, AHCI_REG_PxFB, fis_recieve);
    ahci_port_mmio_write32(port, AHCI_REG_PxFBU, 0);

    /*
    CommandHeader_t* headers = (CommandHeader_t*)cmd_list;

    for (uint32_t i = 0; i < 32; i++) {
      headers[i].phys_region_desc_table_lenght = 8;
      headers[i].command_table_base_addr = (uintptr_t)kmem_kernel_alloc(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_IDENTITY);
      headers[i].command_table_base_addr_upper = 0;
    }
    */

    // Reset errors
    ahci_port_mmio_write32(port, AHCI_REG_PxSERR, ahci_port_mmio_read32(port, AHCI_REG_PxSERR));

    // Reset interrupts
    ahci_port_mmio_write32(port, AHCI_REG_PxIE, 0);
    ahci_port_mmio_write32(port, AHCI_REG_PxIS, ahci_port_mmio_read32(port, AHCI_REG_PxIS));

    uint32_t tfd = ahci_port_mmio_read32(port, AHCI_REG_PxTFD);

    if ((tfd & (AHCI_PxTFD_DRQ | AHCI_PxTFD_BSY)) == 0) {
        if (port_has_phy(port)) {
            // Device connected, let's ask it for it's info

            return ANIVA_SUCCESS;
        }
    }

    port_stop_fis_recieving(port);

    return ANIVA_FAIL;
}

ANIVA_STATUS ahci_port_gather_info(ahci_port_t* port)
{
    disk_dev_t* device = find_gdisk_device(port->m_generic->m_uid);

    ASSERT_MSG(device == port->m_generic, "Device to generic port device mismatch (ahci)");

    /* Activate port */
    port_set_active(port);

    /* Fully enable interrupts */
    ahci_port_mmio_write32(port, AHCI_REG_PxIE, AHCI_PORT_INTERRUPT_FULL_ENABLE);

    /* Prepare a buffer for the identify data */
    ata_identify_block_t* dev_identify_buffer;

    /* Allocate a buffer for us */
    ASSERT(__kmem_kernel_alloc_range((void**)&dev_identify_buffer, SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA) == 0);

    paddr_t dev_ident_phys = kmem_to_phys(nullptr, (vaddr_t)dev_identify_buffer);

    /* Send the identify command to the device */
    if (ahci_port_send_command(port, AHCI_COMMAND_IDENTIFY_DEVICE, dev_ident_phys, 512, false, 0, 0) == ANIVA_FAIL || ahci_port_await_dma_completion_sync(port) == ANIVA_FAIL) {
        goto fail_and_dealloc;
    }

    /* dev_type should be zero */
    if (dev_identify_buffer->general_config.dev_type) {
        goto fail_and_dealloc;
    }

    // Default sector size
    port->m_generic->m_logical_sector_size = 512;
    port->m_generic->m_physical_sector_size = 512;

    uint16_t psstlss = dev_identify_buffer->physical_sector_size_to_logical_sector_size;

    /* Check if this drive has non-default values for sectorsizes */
    if ((psstlss >> 14) == 1) {
        /* Let's use the value disk tells us about */
        if (psstlss & (1 << 12)) {
            port->m_generic->m_logical_sector_size = dev_identify_buffer->logical_sector_size;
        }
        /* Same goes for physical size */
        if (psstlss & (1 << 13)) {
            port->m_generic->m_physical_sector_size = port->m_generic->m_logical_sector_size << (psstlss & 0xF);
        }
    }

    /* Compute most effective transfer size */
    disk_set_effective_sector_count(port->m_generic, dev_identify_buffer->maximum_logical_sectors_per_drq);

    /* Find how large the disk is */
    if (dev_identify_buffer->commands_and_feature_sets_supported[1] & (1 << 10)) {
        port->m_generic->m_max_blk = dev_identify_buffer->user_addressable_logical_sectors_count;
    } else {
        port->m_generic->m_max_blk = dev_identify_buffer->max_28_bit_addressable_logical_sector;
    }

    memcpy(port->m_generic->m_firmware_rev, dev_identify_buffer->firmware_revision, sizeof(uint16_t) * 4);

    /* Give the device its name */
    memcpy(port->m_device_model, dev_identify_buffer->model_number, 40);

    // FIXME: is this specific to AHCI, or a generic ATA thing?
    decode_disk_model_number(port->m_device_model);

    port->m_generic->m_device_name = port->m_device_model;

    /* Try to detect the partitionig type */
    ASSERT_MSG(diskdev_populate_partition_table(port->m_generic) == 0, "Failed to populate partition table!");

    /* Debug information */
    KLOG_DBG("AHCI: Device (Name: %s) attached to port %d. Block count: 0x%llx, Blocksize: 0x%llx\n",
        port->m_device_model,
        port->m_port_index,
        port->m_generic->m_max_blk,
        port->m_generic->m_logical_sector_size);

    /* Clean the buffer */
    __kmem_kernel_dealloc((vaddr_t)dev_identify_buffer, SMALL_PAGE_SIZE);
    // kdebug();
    return ANIVA_SUCCESS;

fail_and_dealloc:
    kernel_panic("DEV IDENTIFY WENT WRONG");
    __kmem_kernel_dealloc((vaddr_t)dev_identify_buffer, SMALL_PAGE_SIZE);
    return ANIVA_FAIL;
}

/*!
 * @brief Handle an IRQ for this port
 *
 * Nothing to add here...
 */
ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port)
{

    port->m_awaiting_dma_transfer_complete = false;

    ahci_port_mmio_write32(port, AHCI_REG_PxIS, 0);

    return ANIVA_SUCCESS;
}

// TODO: check capabilities and crap
static ALWAYS_INLINE void port_spinup(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status |= (1 << 1);
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_power_on(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

    if (!(cmd_and_status & (1 << 20)))
        return;

    cmd_and_status |= (1 << 2);
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_start_clp(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status |= (1 << 0);
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status &= 0xfffffffe;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status |= AHCI_PxCMD_FRE;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port)
{
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status &= ~AHCI_PxCMD_FRE;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_set_active(ahci_port_t* port)
{

    while (ahci_port_mmio_read32(port, AHCI_REG_PxCMD) & AHCI_PxCMD_CR)
        udelay(500);

    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status |= AHCI_PxCMD_ST;
    cmd_and_status |= AHCI_PxCMD_FRE;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port)
{

    // Zero CMD.ST
    uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    cmd_and_status &= ~AHCI_PxCMD_ST;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);

    // Wait for CMD.CR to clear
    while (cmd_and_status & AHCI_PxCMD_CR) {
        cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
    }

    // Finally zero CMD.FRE
    cmd_and_status &= ~AHCI_PxCMD_FRE;
    ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);

    // TODO: Spec states software should wait at least 500 miliseconds. We
    // can abort after this time has passed + some buffer time perhaps
    while (true) {
        cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

        if ((cmd_and_status & AHCI_PxCMD_FR) || (cmd_and_status & AHCI_PxCMD_CR))
            continue;

        break;
    }
}
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port)
{
    uint32_t sata_stat = ahci_port_mmio_read32(port, AHCI_REG_PxSSTS);
    return (sata_stat & 0x0f) == 3;
}

/*
 * TODO: implement
 */

int ahci_port_read(device_t* port, void* buffer, disk_offset_t offset, size_t size)
{

    kernel_panic("TODO: implement async ahci read");

    return -1;
}

int ahci_port_write(device_t* port, void* buffer, disk_offset_t offset, size_t size)
{

    kernel_panic("TODO: implement async ahci write");

    return -1;
}

static int ahci_port_flush(device_t* device)
{
    kernel_panic("TODO: ahci_port_flush");
}

/*!
 * @brief: Write a block to our AHCI port
 *
 * In de device identify phase, we have retrieved information about how many blocks this device likes to give us at once. This means
 * we have two 'blocksizes': a logical blocksize (which is how blocks are addressed on disk) and a effective blocksize (which is the
 * 'optimal' amount of blocks we can transfer at a time times the logical blocksize).
 *
 */
static int ahci_port_write_blk(device_t* device, void* buffer, uintptr_t blk, size_t count)
{
    ahci_port_t* port;
    disk_dev_t* disk_dev;
    paddr_t phys_tmp;
    size_t buffer_size;
    ANIVA_STATUS status;

    if (!device || !count || !buffer)
        return -1;

    disk_dev = device->private;

    if (!disk_dev)
        return -2;

    // Prepare the parent port
    port = disk_dev->m_priv;

    if (!port)
        return -2;

    /* Won't ever fit lmao */
    if (blk > disk_dev->m_max_blk)
        return -3;

    /* FIXME: Should we try to fit count into the disk when it overflows? */

    /* Calculate buffersize */
    buffer_size = count * disk_dev->m_logical_sector_size;

    /* Preallocate a buffer (TODO: contact a generic disk block cache) */
    // tmp = Must(__kmem_kernel_alloc_range(buffer_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
    phys_tmp = kmem_to_phys(nullptr, (uintptr_t)buffer);

    if (phys_tmp == NULL)
        return -4;

    /* Send the AHCI command */
    status = ahci_port_send_command(port, AHCI_COMMAND_WRITE_DMA_EXT, phys_tmp, buffer_size, false, blk, count);

    if (status == ANIVA_FAIL || ahci_port_await_dma_completion_sync(port) == ANIVA_FAIL)
        return -5;

    return 0;
}

/*!
 * @brief: Read a block from our AHCI port
 *
 * In de device identify phase, we have retrieved information about how many blocks this device likes to give us at once. This means
 * we have two 'blocksizes': a logical blocksize (which is how blocks are addressed on disk) and a effective blocksize (which is the
 * 'optimal' amount of blocks we can transfer at a time times the logical blocksize).
 *
 * NOTE/FIXME: could we use @buffer directly?
 * Another NOTE: Yes we prob. could do that, IF we build another framework around the diskdevice interface, which makes sure
 * that @buffer is allocated with KMEM_FLAG_DMA. This would be coupled with persistent disk caches in a centralised place, much
 * like we have in the bootloader (Which has a better disk-interface than us lmaooo)
 */
static int ahci_port_read_blk(device_t* device, void* buffer, uintptr_t blk, size_t count)
{
    ahci_port_t* port;
    disk_dev_t* disk_dev;
    paddr_t phys_tmp;
    size_t buffer_size;
    ANIVA_STATUS status;

    if (!device || !count || !buffer)
        return -1;

    // Prepare the parent port
    disk_dev = device->private;

    if (!disk_dev)
        return -2;

    port = disk_dev->m_priv;

    if (!port)
        return -2;

    /* Won't ever fit lmao */
    if (blk > disk_dev->m_max_blk)
        return -3;

    /* FIXME: Should we try to fit count into the disk when it overflows? */

    /* Calculate buffersize */
    buffer_size = count * disk_dev->m_logical_sector_size;

    /* Preallocate a buffer (TODO: contact a generic disk block cache) */
    // tmp = Must(__kmem_kernel_alloc_range(buffer_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
    phys_tmp = kmem_to_phys(nullptr, (uintptr_t)buffer);

    if (phys_tmp == NULL)
        return -4;

    /* Send the AHCI command */
    status = ahci_port_send_command(port, AHCI_COMMAND_READ_DMA_EXT, phys_tmp, buffer_size, false, blk, count);

    if (status == ANIVA_FAIL || ahci_port_await_dma_completion_sync(port) == ANIVA_FAIL)
        return -5;

    return 0;
}

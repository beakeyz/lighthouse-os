#include "hcd.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "dev/usb/usb.h"
#include "libk/data/bitmap.h"
#include "sync/mutex.h"

/*!
 * @brief Allocate the needed memory for a USB hub structure
 *
 */
usb_hcd_t* create_usb_hcd(struct drv_manifest* driver, pci_device_t* host, char* hub_name, device_ctl_node_t* ctllist)
{
    usb_hcd_t* ret;
    device_t* device;

    if (!host || !host->dev)
        return nullptr;

    ret = alloc_usb_hcd();

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(usb_hcd_t));

    /* Start with a reference of one */
    ret->ref = 1;
    ret->driver = driver;
    ret->pci_device = host;

    device = host->dev;

    mutex_lock(device->lock);

    /* Take the device from (most likely) PCI */
    driver_takeover_device(driver, device, hub_name, NULL, ret);

    /* Implement the needed USB control codes */
    device_impl_ctl_n(device, driver, ctllist);

    mutex_unlock(device->lock);

    ret->devaddr_bitmap = create_bitmap_ex(128, 0x00);

    if (!ret->devaddr_bitmap)
        goto dealloc_and_exit;

    /* Reserve zero */
    bitmap_mark(ret->devaddr_bitmap, 0);

    ret->hcd_lock = create_mutex(NULL);

    return ret;

dealloc_and_exit:
    dealloc_usb_hcd(ret);
    return nullptr;
}

/*!
 * @brief Destroy the hubs resources and the hub itself
 *
 */
void destroy_usb_hcd(usb_hcd_t* hub)
{
    destroy_device(hub->pci_device->dev);
    destroy_mutex(hub->hcd_lock);
    dealloc_usb_hcd(hub);
}

/*!
 * @brief: Allocate a deviceaddress for a usb device
 */
int usb_hcd_alloc_devaddr(usb_hcd_t* hcd, uint8_t* paddr)
{
    uint64_t addr;
    kerror_t res;

    if (!paddr)
        return -1;

    *paddr = 0x00;

    if (!hcd)
        return -1;

    res = bitmap_find_free(hcd->devaddr_bitmap, &addr);

    if (res)
        return -1;

    /* Outside of the device address range */
    if (addr >= 128)
        return -1;

    /* Mark as used */
    bitmap_mark(hcd->devaddr_bitmap, addr);

    /* Export the address */
    *paddr = (uint8_t)(addr);

    return 0;
}

/*!
 * @brief: Deallocate a device address from a hub
 *
 * Called when a device is removed from the hub
 */
int usb_hcd_dealloc_devaddr(usb_hcd_t* hcd, uint8_t addr)
{
    if (!addr)
        return -1;

    if (!bitmap_isset(hcd->devaddr_bitmap, addr))
        return -1;

    bitmap_unmark(hcd->devaddr_bitmap, addr);
    return 0;
}

bool hcd_is_accessed(struct usb_hcd* hcd)
{
    return (hcd->ref);
}

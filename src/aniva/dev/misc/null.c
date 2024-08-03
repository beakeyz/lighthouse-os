#include "null.h"
#include "dev/device.h"
#include "dev/manifest.h"
#include "devices/shared.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "system/profile/profile.h"
#include <libk/string.h>

device_t* null_device;

int null_read(device_t* device, u64 offset, void* buffer, size_t size)
{
    return 0;
}

int null_write(device_t* device, drv_manifest_t* driver, u64 offset, void* buffer, size_t size)
{
    return 0;
}

int null_devinfo(device_t* device, drv_manifest_t* driver, u64 offset, DEVINFO* binfo, size_t bsize)
{
    memset(binfo, 0, sizeof(*binfo));

    strncpy((char*)binfo->devicename, "No device lmao", sizeof(binfo->devicename));
    strncpy((char*)binfo->manufacturer, "Not Beakey, idk who made dis", sizeof(binfo->manufacturer));

    binfo->vendorid = 0;
    binfo->deviceid = 0;

    binfo->class = 420;
    binfo->subclass = 0x6969;
    binfo->ctype = DEVICE_CTYPE_SOFTDEV;

    return 0;
}

static device_ctl_node_t null_ctl[] = {
    DEVICE_CTL(DEVICE_CTLC_READ, null_read, NULL),
    DEVICE_CTL(DEVICE_CTLC_WRITE, null_write, NULL),
    DEVICE_CTL(DEVICE_CTLC_GETINFO, null_devinfo, NULL),
    DEVICE_CTL_END,
};

void init_null_device(oss_node_t* dev_node)
{
    /* ??? */
    if (!dev_node)
        return;

    null_device = create_device_ex(NULL, "Null", NULL, NULL, null_ctl);

    if (!null_device)
        return;

    /* Reset the priv level, so anyone can access this device */
    oss_obj_set_priv_levels(null_device->obj, PRIV_LVL_USER);

    oss_node_add_obj(dev_node, null_device->obj);
}

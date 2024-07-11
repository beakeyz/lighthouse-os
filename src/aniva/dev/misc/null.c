#include "null.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "devices/shared.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "system/profile/profile.h"
#include <libk/string.h>

device_t* null_device;

int null_read(device_t* device, void* buffer, u64 offset, size_t size)
{
    return 0;
}

int null_write(device_t* device, void* buffer, u64 offset, size_t size)
{
    return 0;
}

int null_devinfo(device_t* device, DEVINFO* binfo)
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

struct device_generic_endpoint generic_ep = {
    NULL,
    .f_get_devinfo = null_devinfo,
    .f_read = null_read,
    .f_write = null_write,
};

device_ep_t null_eps[] = {
    DEVICE_ENDPOINT(ENDPOINT_TYPE_GENERIC, generic_ep),
    { NULL },
};

void init_null_device(oss_node_t* dev_node)
{
    /* ??? */
    if (!dev_node)
        return;

    null_device = create_device_ex(NULL, "Null", NULL, NULL, null_eps);

    if (!null_device)
        return;

    /* Reset the priv level, so anyone can access this device */
    oss_obj_set_priv_levels(null_device->obj, PRIV_LVL_USER);

    oss_node_add_obj(dev_node, null_device->obj);
}

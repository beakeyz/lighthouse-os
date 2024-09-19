#include "devices/device.h"
#include "devices/shared.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include <stdio.h>

int main()
{
    printf("Testing %%/Dev/Null\n");
    return -1;

    DEVINFO info;
    DEV_HANDLE device = open_device("Dev/Null", HNDL_FLAG_RW);

    if (!handle_verify(device))
        return -1;

    if (!device_query_info(device, &info))
        return -2;

    printf("Got device: %s\n", info.devicename);
    printf("Device manufacturer: %s\n", info.manufacturer);
    printf("Device class: %d\n", info.class);
    printf("Device subclass: %d\n", info.subclass);
    printf("Device ctype: %s\n", devinfo_get_ctype(info.ctype));

    return 0;
}

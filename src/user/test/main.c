#include "lightos/dev/device.h"
#include "lightos/dev/shared.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/memory/memmap.h"
#include "lightos/memory/memory.h"
#include <stdio.h>

int main()
{
    printf("Testing %%/Dev/Null\n");

    DEVINFO info;
    DEV_HANDLE device = open_device("Dev/Null", HNDL_FLAG_RW);

    if (handle_verify(device))
        return -1;

    if (!device_query_info(device, &info))
        return -2;

    printf("Got device: %s\n", info.devicename);
    printf("Device manufacturer: %s\n", info.manufacturer);
    printf("Device class: %d\n", info.class);
    printf("Device subclass: %d\n", info.subclass);
    printf("Device ctype: %s\n", devinfo_get_ctype(info.ctype));

    printf(" [ TESTING MEMMAP STUFF ]\n");

    /* Create a new mapping */
    void* result;
    HANDLE mapping = vmem_open("epic mapping", HNDL_FLAG_RW, HNDL_MODE_CREATE);

    /* Create a mapping */
    if (vmem_map(mapping, &result, NULL, 0x1000, VMEM_FLAG_READ | VMEM_FLAG_WRITE | VMEM_FLAG_SHARED))
        return -ENOMEM;

    printf("Got a mapping: 0x%p\n", result);

    return 0;
}

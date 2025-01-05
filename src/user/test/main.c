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
    u32* result;
    size_t size;
    page_range_t range;
    HANDLE mapping = vmem_open("Admin/usbmntr/vmem/COUNT", HNDL_FLAG_RW, HNDL_MODE_NORMAL);

    if (handle_verify(mapping))
        return -EINVAL;

    /* Create a new mapping */
    if (vmem_remap(mapping, nullptr, (void**)&result, &size, VMEM_FLAG_READ | VMEM_FLAG_WRITE | VMEM_FLAG_SHARED, nullptr))
        return -EPERM;

    printf("Got a mapping: 0x%p\n", result);

    /* Get mapping lol */
    if (vmem_get_range(mapping, &range))
        return -ENODEV;

    /* Debug */
    printf("Range(page_idx=0x%llx, page_nr=%d, references=%d, flags=0x%x)\n", range.page_idx, range.nr_pages, range.refc, range.flags);
    *result = 10;

    return 0;
}

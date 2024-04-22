#include "devacs/device.h"
#include "lightos/handle.h"
#include "lightos/memory/memflags.h"
#include <sys/types.h>
#include <lightos/memory/alloc.h>
#include <lightos/fs/dir.h>
#include <stdio.h>

/*
 * What should the init process do?
 *  - Verify system integrity before bootstrapping further
 *  - Find various config files and load the ones it needs
 *  - Try to configure the kernel trough the essensial configs
 *  - Find the vector of services to run, like in no particular order
 *      -- Display manager
 *      -- Sessions
 *      -- Network services
 *      -- Peripherals
 *      -- Audio
 *  - Find out if we need to install the system on a permanent storage device and
 *    if so, run the installer (which creates the partition, installs the fs and the system)
 *  - Find the vector of further bootstrap applications to run and run them
 */
int main() 
{
  DEVINFO info;
  DEV_HANDLE handle;

  const char* device = "Dev/ahci/drive0";

  printf("Querying: %s\n", device);

  handle = open_device(device, NULL);

  if (handle == HNDL_NO_PERM) {
    printf("Could not open this handle! (No permission)\n");
    return -1;
  }

  if (!handle_verify(handle))
    return -1;

  if (!device_query_info(handle, &info))
    goto close_and_end;

  printf("Name: %s\n", info.devicename);
  printf("Vendor ID: %d\n", info.vendorid);
  printf("Device ID: %d\n", info.deviceid);
  printf("Class: %d\n", info.class);
  printf("Subclass: %d\n", info.subclass);
  printf("Device state: %s\n", device_enable(handle) ? "Enabled" : "Disabled");

  size_t size = 0x800000000;

  //for (uint32_t i = 0; i < 0xffffffff; i++) {
    //printf("Allocating %lld bytes\n", size);
    //allocate_pool(&size, MEMPOOL_FLAG_RW, MEMPOOL_TYPE_BUFFER);
  //}

  allocate_pool(&size, MEMPOOL_FLAG_RW, MEMPOOL_TYPE_BUFFER);

close_and_end:
  close_device(handle);
  return 0;
}

#include "devacs/device.h"
#include "lightos/dynamic.h"
#include "lightos/handle.h"
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
  HANDLE devlib;
  DEV_HANDLE handle;

  DEV_HANDLE (*f_open_dev)(const char* path, uint32_t flags);
  bool (*f_close_dev)(DEV_HANDLE handle);
  bool (*f_query)(DEV_HANDLE handle, DEVINFO* binfo);
  bool (*f_enable)(DEV_HANDLE handle);

  if (!load_library("devacs.slb", &devlib))
    return -1;

  if (!get_func_address(devlib, "open_device", (void**)&f_open_dev))
    return -2;

  if (!get_func_address(devlib, "close_device", (void**)&f_close_dev))
    return -3;

  if (!get_func_address(devlib, "device_query_info", (void**)&f_query))
    return -4;

  if (!get_func_address(devlib, "device_enable", (void**)&f_enable))
    return -5;

  const char* device = "Dev/ahci/drive0";

  printf("Querying: %s\n", device);

  handle = f_open_dev(device, NULL);

  if (handle == HNDL_NO_PERM) {
    printf("Could not open this handle! (No permission)\n");
    return -1;
  }

  if (!handle_verify(handle))
    return -1;

  if (!f_query(handle, &info))
    goto close_and_end;

  printf("Name: %s\n", info.devicename);
  printf("Vendor ID: %d\n", info.vendorid);
  printf("Device ID: %d\n", info.deviceid);
  printf("Class: %d\n", info.class);
  printf("Subclass: %d\n", info.subclass);
  printf("Device state: %s\n", f_enable(handle) ? "Enabled" : "Disabled");

close_and_end:
  f_close_dev(handle);
  return 0;
}

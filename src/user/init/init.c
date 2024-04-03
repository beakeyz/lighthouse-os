#include <sys/types.h>
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
  uint32_t idx;
  DirEntry* entry;
  Directory* apps = open_dir("Dev/haha", HNDL_FLAG_R, NULL);

  if (!apps)
    return -1;

  idx = 0;

  do {
    entry = dir_read_entry(apps, idx++);

    if (entry)
      printf("(0x%lld) %s\n", (uintptr_t)entry, entry->name);

  } while (entry);

  close_dir(apps);
  return 0;
}

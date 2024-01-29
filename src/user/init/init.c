
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/proc/profile.h"
#include "lightos/proc/var_types.h"
#include <lightos/system.h>
#include <lightos/syscall.h>
#include <stdio.h>
#include <stdlib.h>

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
int main() {

  /*
   * FIXME: are we going to give every path root a letter like windows, 
   * or do we just have one root like linux/unix?
   */
  char buffer[128];
  HANDLE h = open_handle(":/Root/Apps/init", HNDL_TYPE_FILE, HNDL_FLAG_RW, NULL);

  if (!handle_verify(h)) 
    return -1;

  printf("Handle: %d\n", h);

  if (stdin->r_buff == stdout->w_buff)
    printf("How the fuck did that happen?\n");

  char* resp = gets(buffer, sizeof(buffer));

  if (resp) printf("Your name is: %s\n", resp);
  else printf("Could not take in that name!\n");

  resp = gets(buffer, sizeof(buffer));

  if (resp) printf("Your age is: %s\n", resp);
  else printf("Could not take in that age!\n");

  char test_buffer[128] = { 0 };

  profile_var_read_ex("Global", "SYSTEM_NAME", HNDL_FLAG_RW, sizeof(test_buffer), test_buffer);

  printf("Tried to read from the profile variable handle: %s\n", test_buffer);

  HANDLE profile;

  profile = open_profile("Global", HNDL_FLAG_RW);

  printf("Opened profile\n");

  if (!handle_verify(profile))
    return -1;

  printf("Create var..\n");
  return create_profile_variable(profile, "Test", PROFILE_VAR_TYPE_STRING, NULL, "Test string");
}

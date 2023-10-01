
#include "LibSys/driver/drv.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include "LibSys/proc/process.h"
#include "LibSys/proc/profile.h"
#include "LibSys/proc/socket.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>
#include <assert.h>
#include <stdint.h>
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

  /* Open a handle to the binary file of our own process */
  HANDLE handle_1 = open_handle("Root/init", HNDL_TYPE_FILE, HNDL_FLAG_RW, NULL);

  printf("open Root/init!\n");

  /* Open a handle to our own process */
  HANDLE handle_2 = open_handle("init", HNDL_TYPE_PROC, NULL, NULL);

  printf("open init proc!\n");

  uint32_t* memory = malloc(sizeof(uint32_t));

  assert(memory);

  if (memory)
    *memory = 69;

  printf("Yay memory\n");

  char buffer[128];

  //scanf("Whats your name: %s", buffer);
  char* resp = gets(buffer, sizeof(buffer));

  if (resp)
    printf("Your name is: %s\n", resp);
  else 
    printf("Could not take in that name!\n");

  char test_buffer[128] = { 0 };

  BOOL result = profile_var_read_ex("Global", "SYSTEM_NAME", HNDL_FLAG_RW, sizeof(test_buffer), test_buffer);

  printf("Tried to read from the profile variable handle!\n");

  free(memory);
  return handle_2;
}

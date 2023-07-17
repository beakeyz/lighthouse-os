
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include "LibSys/proc/process.h"
#include "LibSys/proc/socket.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>
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
  HANDLE_t handle = open_handle("Root/dummy.txt", HNDL_TYPE_FILE, NULL, NULL);

  printf("open dummy.txt!\n");

  /* Open a handle to the binary file of our own process */
  HANDLE_t handle_1 = open_handle("Root/init", HNDL_TYPE_FILE, HNDL_FLAG_RW, NULL);

  printf("open Root/init!\n");

  /* Open a handle to our own process */
  HANDLE_t handle_2 = open_handle("init", HNDL_TYPE_PROC, NULL, NULL);

  printf("open init proc!\n");

  uint32_t* memory = malloc(sizeof(uint32_t));

  *memory = 0x6969;

  printf("Memory thing\n");

  char buffer[128];

  //scanf("Whats your name: %s", buffer);
  gets(buffer, sizeof(buffer));

  buffer[127] = NULL;

  printf("Your name is: %s", buffer);

  return handle_2;
}

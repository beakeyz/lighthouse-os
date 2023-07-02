
#include "LibSys/handle.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>
#include <stdio.h>
#include <stdlib.h>

int main() {

  /*
   * FIXME: are we going to give every path root a letter like windows, 
   * or do we just have one root like linux/unix?
   */
  HANDLE_t handle = open_handle("Root/dummy.txt", HNDL_TYPE_FILE, NULL, NULL);

  /* Open a handle to the binary file of our own process */
  HANDLE_t handle_1 = open_handle("Root/init", HNDL_TYPE_FILE, NULL, NULL);

  /* Open a handle to our own process */
  HANDLE_t handle_2 = open_handle("init", HNDL_TYPE_PROC, NULL, NULL);

  // FIXME: GPF
  printf("Memory thing\n");

  return handle_2;
}

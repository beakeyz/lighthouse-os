
#include "LibSys/handle.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>

int main() {

  /*
   * FIXME: are we going to give every path root a letter like windows, 
   * or do we just have one root like linux/unix?
   */
  HANDLE_t handle = open_handle("Devices/disk/cramfs/dummy.txt", HNDL_TYPE_FILE, NULL, NULL);
  HANDLE_t handle_1 = open_handle("Devices/disk/cramfs/init", HNDL_TYPE_FILE, NULL, NULL);

  return handle_1;
}

#ifndef __LIGHTENV_PROCESS__
#define __LIGHTENV_PROCESS__

#include "LibSys/system.h"
#include "LibSys/handle_def.h"

#include "stdint.h"
#include "sys/types.h"

/*
 * Open a handle to a process which can be used to communicate with this
 * process or use this handle to reference the process to other services
 */
HANDLE_t open_proc(
  __IN__ const char* name,
  __IN__ DWORD flags,
  __IN__ DWORD mode
);

/*
 * Check if we can communicate with this process
 *
 * Returns false if either we can't communicate with 
 * this process, or if the handle is invalid.
 *
 * Returns true otherwise
 */
BOOL proc_is_public(
  __IN__ HANDLE_t handle
);

/*
 * Try to send a bit of data to the process
 * Fails if the process is not open for communication, busy 
 * of if the handle is invalid
 */
BOOL proc_send_data(
  __IN__    HANDLE_t    handle,
  __IN__    VOID*       buffer,
  __INOUT__ size_t*     buffer_size,
  __IN__    DWORD       flags
);

/*
 * Wait until the process specified by the handle sends us some data.
 * The buffer should specify a region for data that the caller has 
 * prepared, since the data that was recieved will be copied there
 */
BOOL proc_await_data(
  __IN__    HANDLE_t    handle,
  __IN__    VOID*       buffer,
  __IN__    size_t      size
);

/*
 * Write some values into another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the write opperation failed
 */
BOOL write_process_memory(
  __IN__    HANDLE_t    handle,
  __IN__    VOID*       offset,
  __IN__    VOID*       buffer,
  __INOUT__ size_t*     size,
  __IN__    DWORD       flags
);

/*
 * Read from another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the read opperation failed
 */
BOOL read_process_memory(
  __IN__    HANDLE_t    handle,
  __IN__    VOID*       offset,
  __INOUT__ size_t*     size,
  __OUT__   VOID*       buffer,
  __IN__    DWORD       flags
);

#define PROC_ORPHAN   (0x00000001) /* Don't take this process as a child, but rather let it be its own seperate process */

BOOL create_process(
  __IN__ const char*    name,
  __IN__ FuncPtr        entry,
  __IN__ const char*    args,
  __IN__ size_t*        argc,
  __IN__ DWORD          flags
);

BOOL kill_process(
  __IN__ HANDLE_t   handle,
  __IN__ DWORD      flags
);


#endif // !__LIGHTENV_PROCESS__

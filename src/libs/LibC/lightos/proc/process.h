#ifndef __LIGHTENV_PROCESS__
#define __LIGHTENV_PROCESS__

#include "lightos/handle_def.h"
#include "lightos/system.h"

#include "stdint.h"
#include "sys/types.h"

/*
 * Open a handle to a process which can be used to communicate with this
 * process or use this handle to reference the process to other services
 */
extern HANDLE open_proc(
    __IN__ const char* name,
    __IN__ DWORD flags,
    __IN__ DWORD mode);

extern HANDLE open_proc_ex(
    __IN__ DWORD procid,
    __IN__ DWORD flags,
    __IN__ DWORD mode);

/*
 * Check if we can communicate with this process
 *
 * Returns false if either we can't communicate with
 * this process, or if the handle is invalid.
 *
 * Returns true otherwise
 */
extern BOOL proc_is_public(
    __IN__ HANDLE handle);

typedef DWORD control_code_t;
typedef BYTE privilige_lvl_t;

/*
 * Checks the privilige level of the process handle
 *
 * Fails if the handle or the buffer pointer is invalid or if we have no permission to view the
 * processes privilige level
 */
extern BOOL proc_get_priv_lvl(
    __IN__ HANDLE handle,
    __OUT__ privilige_lvl_t* level);

/*
 * Grab a handle to the profile of this process
 */
extern BOOL proc_get_profile(
    __IN__ HANDLE proc_handle,
    __OUT__ HANDLE* profile_handle);

/*
 * Try to send a bit of data to the process
 * Fails if the process is not open for communication, busy
 * of if the handle is invalid
 */
extern BOOL proc_send_data(
    __IN__ HANDLE handle,
    __IN__ control_code_t code,
    __IN__ __OPTIONAL__ VOID* buffer,
    __INOUT__ __OPTIONAL__ size_t* buffer_size,
    __IN__ DWORD flags);

/*
 * Wait until the process specified by the handle sends us some data.
 * The buffer should specify a region for data that the caller has
 * prepared, since the data that was recieved will be copied there
 */
extern BOOL proc_await_data(
    __IN__ HANDLE handle,
    __IN__ VOID* buffer,
    __IN__ size_t size);

/*
 * Write some values into another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the write opperation failed
 */
extern BOOL write_process_memory(
    __IN__ HANDLE handle,
    __IN__ VOID* offset,
    __IN__ VOID* buffer,
    __INOUT__ size_t* size,
    __IN__ DWORD flags);

/*
 * Read from another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the read opperation failed
 */
extern BOOL read_process_memory(
    __IN__ HANDLE handle,
    __IN__ VOID* offset,
    __INOUT__ size_t* size,
    __OUT__ VOID* buffer,
    __IN__ DWORD flags);

/* Don't take this process as a child, but rather let it be its own seperate process */
#define PROC_ORPHAN (0x00000001)

extern BOOL create_process(
    __IN__ const char* name,
    __IN__ FuncPtr entry,
    __IN__ const char** args,
    __IN__ size_t argc,
    __IN__ DWORD flags);

extern BOOL kill_process(
    __IN__ HANDLE handle,
    __IN__ DWORD flags);

/*
 * Find the amount of ticks used by this process
 */
extern size_t get_process_time();

#endif // !__LIGHTENV_PROCESS__

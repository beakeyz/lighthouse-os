#ifndef __LIGHTENV_PROCESS__
#define __LIGHTENV_PROCESS__

#include "lightos/api/handle.h"

#include "stdint.h"
#include "sys/types.h"

/*
 * Open a handle to a process which can be used to communicate with this
 * process or use this handle to reference the process to other services
 */
extern HANDLE open_proc(const char* name, u32 flags, u32 mode);

extern HANDLE open_proc_ex(
    u32 procid,
    u32 flags,
    u32 mode);

/*
 * Check if we can communicate with this process
 *
 * Returns false if either we can't communicate with
 * this process, or if the handle is invalid.
 *
 * Returns true otherwise
 */
extern BOOL proc_is_public(HANDLE handle);

typedef u32 control_code_t;
typedef u8 privilige_lvl_t;

/*
 * Checks the privilige level of the process handle
 *
 * Fails if the handle or the buffer pointer is invalid or if we have no permission to view the
 * processes privilige level
 */
extern BOOL proc_get_priv_lvl(HANDLE handle, privilige_lvl_t* level);

/*
 * Grab a handle to the profile of this process
 */
extern BOOL proc_get_profile(HANDLE proc_handle, HANDLE* profile_handle);

/*
 * Try to send a bit of data to the process
 * Fails if the process is not open for communication, busy
 * of if the handle is invalid
 */
extern BOOL proc_send_data(HANDLE handle, control_code_t code, VOID* buffer, size_t* buffer_size, u32 flags);

/*
 * Wait until the process specified by the handle sends us some data.
 * The buffer should specify a region for data that the caller has
 * prepared, since the data that was recieved will be copied there
 */
extern BOOL proc_await_data(HANDLE handle, VOID* buffer, size_t size);

/*
 * Write some values into another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the write opperation failed
 */
extern BOOL write_process_memory(HANDLE handle, VOID* offset, VOID* buffer, size_t* size, u32 flags);

/*
 * Read from another processes memory
 * Fails if we don't have permission, if the handle is invalid
 * or if for any reason the read opperation failed
 */
extern BOOL read_process_memory(HANDLE handle, VOID* offset, size_t* size, VOID* buffer, u32 flags);

/* Don't take this process as a child, but rather let it be its own seperate process */
#define PROC_ORPHAN (0x00000001)

extern BOOL create_process(const char* name, FuncPtr entry, const char** args, size_t argc, u32 flags);

extern BOOL kill_process(HANDLE handle, u32 flags);

/*!
 * @brief: Gets the time in MS since this process has been running
 *
 */
extern size_t get_process_time();

#endif // !__LIGHTENV_PROCESS__

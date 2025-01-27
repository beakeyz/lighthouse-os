#ifndef __LIGHTENV_HANDLE__
#define __LIGHTENV_HANDLE__

/*
 * A handle is essensially an index into the table that the kernel
 * keeps track of for us. It contains all kinds of references to things
 * a user process can have access to, like files, processes, drivers,
 * structures, ect. A userprocess cant do anything with these handles themselves, but instead
 * they can ask the kernel to do things on their behalf
 *
 * negative handles mean invalid handles of different types
 */
#include "stdint.h"

/* This file contains definitions to be used for both userspace and kernelspace */
#include "lightos/api/handle.h"
#include "lightos/types.h"

/*!
 * @brief: Check if a certain handle is actually valid for use
 */
error_t handle_verify(handle_t handle);

/*
 * Ask the kernel what kind of handle we are dealing with
 */
error_t handle_get_type(HANDLE handle, HANDLE_TYPE* type);

/*
 * Close this handle and make the kernel deallocate its resources
 */
error_t close_handle(HANDLE handle);

/*
 * Simply opens a handle to whatever hapens to be at
 * the specified path. This could be:
 *  - A file in the filesystem, either from the relative root of this process, or the absolute root of the fs
 *  - A process by its process name
 *  - A driver by its driver name
 *  - ect.
 * NOTE: this is the raw function that should really only be called by the api itself, but this
 * can be used in userspace itself if handled carefully
 */
HANDLE open_handle(const char* path, HANDLE_TYPE type, u32 flags, enum HNDL_MODE mode);

/*!
 * @brief: Open a handle relative to another handle
 *
 * This will always return a handle of the type HNDL_TYPE_OSS_OBJ.
 * The handletype can be expanded by handle_expand_type
 */
HANDLE open_handle_from(HANDLE rel_handle, const char* path, HANDLE_TYPE type, u32 flags, enum HNDL_MODE mode);

/*
 * Perform a read opperation on a handle
 */
ssize_t handle_read(HANDLE handle, u64 offset, VOID* buffer, u64 buffer_size);

/*
 * Perform a write opperation on a handle
 */
error_t handle_write(HANDLE handle, u64 offset, VOID* buffer, size_t buffer_size);

#endif // !__LIGHTENV_HANDLE__

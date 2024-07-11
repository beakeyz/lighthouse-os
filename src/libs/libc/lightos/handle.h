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
#include "lightos/system.h"
#include "stdint.h"

/* This file contains definitions to be used for both userspace and kernelspace */
#include "handle_def.h"
#include "sys/types.h"

/*
 * Check if a certain handle is actually valid for use
 */
BOOL handle_verify(
    __IN__ handle_t handle);

/*
 * Tell the kernel we want to expand our
 * capacity for handles. We should pose a hard limit though
 * (hard max is around 1024 per process)
 * When a process needs more than 1000 handles I think we can
 * assume that they are doing something weird lol (unless that process is a service / driver )
 */
BOOL expand_handle_capacity(
    __IN__ size_t extra_size);

/*
 * Ask the kernel what kind of handle we are dealing with
 */
BOOL get_handle_type(
    __IN__ HANDLE handle,
    __OUT__ HANDLE_TYPE* type);

/*!
 * @brief: Tries to 'expand' the type of a certain handle into something more specific
 *
 * Some types (Like HNDL_TYPE_OSS_OBJ) are 'basic' handle types, which can have multiple underlying
 * objects, like files, devices, drivers, ect. 
 */
BOOL handle_expand_type(HANDLE handle);
BOOL handle_reclassify(HANDLE handle, HANDLE_TYPE type);

/*
 * Close this handle and make the kernel deallocate its resources
 */
BOOL close_handle(
    __IN__ HANDLE handle);

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
HANDLE open_handle(
    __IN__ const char* path,
    __IN__ __OPTIONAL__ HANDLE_TYPE type,
    __IN__ DWORD flags,
    __IN__ DWORD mode);

/*!
 * @brief: Open a handle relative to another handle
 *
 * This will always return a handle of the type HNDL_TYPE_OSS_OBJ.
 * The handletype can be expanded by handle_expand_type
 */
HANDLE open_handle_rel(
    __IN__ HANDLE rel_handle,
    __IN__ const char* path,
    __IN__ DWORD flags,
    __IN__ DWORD mode);

/*
 * Clone a handle that the process of the caller owns. The properties
 * of the duplicated handle will stay the same and it will also point to
 * the same object, but its value will be different
 */
BOOL duplicate_local_handle(
    __IN__ HANDLE src_handle,
    __OUT__ HANDLE* out_handle);

/*
 * Checks to see if the value of this handle has changed for some unforseen reason
 */
BOOL update_handle(
    __IN__ HANDLE handle,
    __OUT__ HANDLE* check_handle);

/*!
 * @brief: Set the internal offset of a handle
 * @handle: The handle to set the offset of
 * @offset: The offset to set
 *
 * @returns: Wether the offset was successfully set to the desired value
 */
BOOL handle_set_offset(
    __IN__ HANDLE handle,
    __IN__ QWORD offset);

/*!
 * @brief: Get the current offset of a handle
 * @handle: The handle to get the offset of
 * @boffset: The buffer where the offset will be placed
 *
 * @returns: True if we could get the offset successfully, false otherwise
 */
BOOL handle_get_offset(
    __IN__ HANDLE handle,
    __OUT__ QWORD* boffset);

/*
 * Perform a read opperation on a handle
 */
BOOL handle_read(
    __IN__ HANDLE handle,
    __IN__ QWORD buffer_size,
    __OUT__ VOID* buffer);

/*
 * Perform a write opperation on a handle
 */
BOOL handle_write(
    __IN__ HANDLE handle,
    __IN__ QWORD buffer_size,
    __IN__ VOID* buffer);

#endif // !__LIGHTENV_HANDLE__

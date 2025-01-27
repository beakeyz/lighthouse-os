#ifndef __LIGHTENV_SYSCALL__
#define __LIGHTENV_SYSCALL__

/*
 * This file contains the ids for the various syscalls in the aniva kernel, as well
 * as the different flags that come with them. This header is used both by the kernel,
 * as the userspace, so make sure this stays managable
 *
 * IDs are prefixed with            SYSID_
 * status codes are prefixed with   SYS_
 *
 * TODO: Terminate reduntant syscall ids, due to the new khdriver model
 * TODO: Since SYSID_OPEN requires a HNDL_MODE to be passed, all SYSID_CREATE_*
 * sysids must be terminated
 */

#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/api/process.h"
#include "lightos/api/sysvar.h"
#include <lightos/types.h>

/*!
 * @brief: Standard definition for a system function
 */
typedef uint64_t (*sys_fn_t)(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4);

enum SYSID {
    SYSID_INVAL,
    SYSID_EXIT, /* Exit the process */
    SYSID_GET_EXITVEC, /* Gets the exitvector */
    SYSID_CLOSE, /* Close a handle */
    SYSID_READ, /* Read from a handle */
    SYSID_WRITE, /* Write to a handle */
    SYSID_OPEN, /* Open/Create kernel objects */
    SYSID_OPEN_IDX, /* Open an oss object based on index */
    SYSID_OPEN_CONNECTED_IDX, /* Open an oss object that's already connected based on index */
    SYSID_SEND_MSG, /* Send an object communication message. Might be a device ctlc */

    SYSID_GET_OBJECT_TYPE, /* Gets the type of an object */
    SYSID_SET_OBJECT_TYPE, /* Tries to set an object type. Fails if the type is already set */
    SYSID_GET_OBJECT_KEY, /* Gets the current object key for a give object */
    SYSID_SET_OBJECT_KEY, /* Tries to set the key for a given object */

    SYSID_ALLOC_VMEM, /* Allocates a range of virtual memory */
    SYSID_DEALLOC_VMEM, /* Deallocates a range of virtual memory */
    SYSID_MAP_VMEM, /* Map a range of virtual memory */
    SYSID_PROTECT_VMEM, /* Change the memory flags of virtual memory */

    SYSID_SYSEXEC, /* Ask the system to do stuff for us */
    SYSID_CREATE_PROC,
    SYSID_DESTROY_PROC,
    SYSID_GET_HNDL_TYPE,
    SYSID_GET_SYSVAR_TYPE,
    SYSID_CREATE_SYSVAR,
    /* Directory syscalls */
    SYSID_DIR_CREATE,
    /* Manipulate the R/W offset of a handle */
    SYSID_SEEK,
    SYSID_GET_PROCESSTIME,
    SYSID_SLEEP,

    /* TODO: */
    SYSID_UNLOAD_DRV,
    SYSID_CREATE_PROFILE,
    SYSID_DESTROY_PROFILE,

    /* Dynamic loader-specific syscalls */
    SYSID_GET_FUNCTION,
};

/* Mask that marks a sysid invalid */
#define SYSID_INVAL_MASK 0x80000000

#define SYSID_IS_VALID(id) (((unsigned int)(id) & SYSID_INVAL_MASK) != SYSID_INVAL_MASK)
#define SYSID_GET(id) (enum SYSID)((unsigned int)(id) & SYSID_INVAL_MASK)
#define SYSID_SET_VALID(id, valid) ({if (valid) id &= ~SYSID_INVAL_MASK; else id |= SYSID_INVAL_MASK; })

/*
 * Per-sysid implementation of raw syscalls
 *
 * These are implemented both in userspace, as in kernel space (with the same symbol names
 * yayyyy)
 */
extern void sys_exit(error_t status);
extern error_t sys_get_exitvec(proc_exitvec_t** p_exitvec);
extern error_t sys_close(HANDLE handle);
extern ssize_t sys_read(HANDLE handle, u64 offset, void* buffer, size_t size);
extern ssize_t sys_write(HANDLE handle, u64 offset, void* buffer, size_t size);
extern HANDLE sys_open(const char* path, handle_flags_t flags, enum HNDL_MODE mode, void* buffer, size_t bsize);
extern HANDLE sys_open_idx(HANDLE handle, u32 idx, handle_flags_t flags);
extern HANDLE sys_open_connected_idx(HANDLE handle, u32 idx, handle_flags_t flags);
extern error_t sys_send_msg(HANDLE handle, u32 code, u64 offset, void* buffer, size_t bsize);

extern enum OSS_OBJECT_TYPE sys_get_object_type(HANDLE handle);
extern enum OSS_OBJECT_TYPE sys_set_object_type(HANDLE handle, enum OSS_OBJECT_TYPE ptype);
extern error_t sys_get_object_key(HANDLE handle, char* key_buff, size_t key_buff_len);
extern error_t sys_set_object_key(HANDLE handle, char* key_buff, size_t key_buff_len);

extern error_t sys_alloc_vmem(size_t size, u32 flags, vaddr_t* paddr);
extern error_t sys_dealloc_vmem(vaddr_t addr, size_t size);
extern void* sys_map_vmem(HANDLE handle, void* addr, size_t len, u32 flags);
extern error_t sys_protect_vmem(void* addr, size_t len, u32 flags);

extern error_t sys_exec(const char* cmd, size_t len);
extern HANDLE sys_create_proc(const char* cmd, FuncPtr entry);
extern error_t sys_destroy_proc(HANDLE proc, u32 flags);

extern enum HANDLE_TYPE sys_handle_get_type(HANDLE handle);
extern enum SYSVAR_TYPE sys_get_sysvar_type(HANDLE handle);
extern HANDLE sys_create_sysvar(const char* key, handle_flags_t flags, enum SYSVAR_TYPE type, void* buffer, size_t len);

extern error_t sys_dir_create(const char* path, i32 mode);

extern size_t sys_seek(HANDLE handle, u64 c_offset, u64 new_offset, u32 type);
extern size_t sys_get_process_time(void);
extern void sys_sleep(u64 ns);

extern void* sys_get_function(HANDLE lib, const char* function);

#endif // !__LIGHTENV_SYSCALL__

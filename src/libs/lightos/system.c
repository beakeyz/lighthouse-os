#include "system.h"
#include "lightos/api/process.h"
#include "lightos/api/sysvar.h"
#include "syscall.h"
#include <lightos/api/handle.h>

extern syscall_result_t syscall_0(syscall_id_t id);
extern syscall_result_t syscall_1(syscall_id_t id, uintptr_t arg0);
extern syscall_result_t syscall_2(syscall_id_t id, uintptr_t arg0, uintptr_t arg1);
extern syscall_result_t syscall_3(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2);
extern syscall_result_t syscall_4(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
extern syscall_result_t syscall_5(syscall_id_t id, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

void sys_exit(error_t status)
{
    syscall_1(SYSID_EXIT, status);
}

error_t sys_get_exitvec(proc_exitvec_t** p_exitvec)
{
    return syscall_1(SYSID_GET_EXITVEC, (u64)p_exitvec);
}

error_t sys_close(HANDLE handle)
{
    return syscall_1(SYSID_CLOSE, handle);
}

ssize_t sys_read(HANDLE handle, u64 offset, void* buffer, size_t size)
{
    return syscall_4(SYSID_READ, handle, offset, (u64)buffer, size);
}

ssize_t sys_write(HANDLE handle, u64 offset, void* buffer, size_t size)
{
    return syscall_4(SYSID_WRITE, handle, offset, (u64)buffer, size);
}

HANDLE sys_open(const char* path, handle_flags_t flags, enum HNDL_MODE mode, void* buffer, size_t bsize)
{
    return syscall_5(SYSID_OPEN, (u64)path, flags.raw, mode, (u64)buffer, bsize);
}

HANDLE sys_open_idx(HANDLE handle, u32 idx, handle_flags_t flags)
{
    return syscall_3(SYSID_OPEN_IDX, handle, idx, flags.raw);
}

HANDLE sys_open_connected_idx(HANDLE handle, u32 idx, handle_flags_t flags)
{
    return syscall_3(SYSID_OPEN_CONNECTED_IDX, handle, idx, flags.raw);
}

error_t sys_send_msg(HANDLE handle, u32 code, u64 offset, void* buffer, size_t bsize)
{
    return syscall_5(SYSID_SEND_MSG, handle, code, offset, (u64)buffer, bsize);
}

enum OSS_OBJECT_TYPE sys_get_object_type(HANDLE handle)
{
    return syscall_1(SYSID_GET_OBJECT_TYPE, handle);
}

enum OSS_OBJECT_TYPE sys_set_object_type(HANDLE handle, enum OSS_OBJECT_TYPE ptype)
{
    return syscall_2(SYSID_SET_OBJECT_TYPE, handle, ptype);
}

error_t sys_get_object_key(HANDLE handle, char* key_buff, size_t key_buff_len)
{
    return syscall_3(SYSID_GET_OBJECT_KEY, handle, (u64)key_buff, key_buff_len);
}

error_t sys_set_object_key(HANDLE handle, char* key_buff, size_t key_buff_len)
{
    return syscall_3(SYSID_SET_OBJECT_KEY, handle, (u64)key_buff, key_buff_len);
}

error_t sys_connect_object(HANDLE object, HANDLE new_parent)
{
    return syscall_2(SYSID_CONNECT_OBJECT, object, new_parent);
}

error_t sys_disconnect_object(HANDLE object, HANDLE parent)
{
    return syscall_2(SYSID_DISCONNECT_OBJECT, object, parent);
}

error_t sys_alloc_vmem(size_t size, u32 flags, vaddr_t* paddr)
{
    return syscall_3(SYSID_ALLOC_VMEM, size, flags, (u64)paddr);
}

error_t sys_dealloc_vmem(vaddr_t addr, size_t size)
{
    return syscall_2(SYSID_DEALLOC_VMEM, addr, size);
}

void* sys_map_vmem(HANDLE handle, void* addr, size_t len, u32 flags)
{
    return (void*)syscall_4(SYSID_MAP_VMEM, handle, (u64)addr, len, flags);
}

error_t sys_protect_vmem(void* addr, size_t len, u32 flags)
{
    return syscall_3(SYSID_PROTECT_VMEM, (u64)addr, len, flags);
}

error_t sys_exec(const char* cmd, size_t len)
{
    return syscall_2(SYSID_SYSEXEC, (u64)cmd, len);
}

HANDLE sys_open_proc_obj(const char* key, handle_flags_t flags)
{
    return syscall_2(SYSID_OPEN_PROC_OBJ, (u64)key, flags.raw);
}

HANDLE sys_create_proc(const char* cmd, FuncPtr entry)
{
    return syscall_2(SYSID_CREATE_PROC, (u64)cmd, (u64)entry);
}

error_t sys_destroy_proc(HANDLE proc, u32 flags)
{
    return syscall_2(SYSID_DESTROY_PROC, proc, flags);
}

enum HANDLE_TYPE sys_handle_get_type(HANDLE handle)
{
    return (enum HANDLE_TYPE)syscall_1(SYSID_GET_HNDL_TYPE, handle);
}

enum SYSVAR_TYPE sys_get_sysvar_type(HANDLE handle)
{
    return (enum SYSVAR_TYPE)syscall_1(SYSID_GET_SYSVAR_TYPE, handle);
}

HANDLE sys_create_sysvar(const char* key, handle_flags_t flags, enum SYSVAR_TYPE type, void* buffer, size_t len)
{
    return (HANDLE)syscall_5(SYSID_CREATE_SYSVAR, (u64)key, flags.raw, type, (u64)buffer, len);
}

error_t sys_dir_create(const char* path, i32 mode)
{
    return syscall_2(SYSID_DIR_CREATE, (u64)path, mode);
}

size_t sys_seek(HANDLE handle, u64 c_offset, u64 new_offset, u32 type)
{
    return syscall_4(SYSID_SEEK, handle, c_offset, new_offset, type);
}

size_t sys_get_process_time(void)
{
    return syscall_0(SYSID_GET_PROCESSTIME);
}

void sys_sleep(u64 ns)
{
    syscall_1(SYSID_SLEEP, ns);
}

void* sys_get_function(HANDLE lib, const char* function)
{
    return (void*)syscall_2(SYSID_GET_FUNCTION, lib, (u64)function);
}

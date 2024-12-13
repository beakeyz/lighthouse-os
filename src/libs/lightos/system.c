#include "system.h"
#include "lightos/sysvar/shared.h"
#include "syscall.h"
#include <lightos/handle_def.h>

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

error_t sys_get_exitvec(dynldr_exit_vector_t** p_exitvec)
{
    return syscall_1(SYSID_GET_EXITVEC, (u64)p_exitvec);
}

error_t sys_close(HANDLE handle)
{
    return syscall_1(SYSID_CLOSE, handle);
}

error_t sys_read(HANDLE handle, void* buffer, size_t size, size_t* pread_size)
{
    return syscall_4(SYSID_READ, handle, (u64)buffer, size, (u64)pread_size);
}

error_t sys_write(HANDLE handle, void* buffer, size_t size)
{
    return syscall_3(SYSID_WRITE, handle, (u64)buffer, size);
}

HANDLE sys_open(const char* path, handle_flags_t flags, enum HNDL_MODE mode, void* buffer, size_t bsize)
{
    return syscall_5(SYSID_OPEN, (u64)path, flags.raw, mode, (u64)buffer, bsize);
}

error_t sys_send_msg(HANDLE handle, u32 code, u64 offset, void* buffer, size_t bsize)
{
    return syscall_5(SYSID_SEND_MSG, handle, code, offset, (u64)buffer, bsize);
}

error_t sys_send_ctl(HANDLE handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    return syscall_5(SYSID_SEND_CTL, handle, code, offset, (u64)buffer, bsize);
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

size_t sys_dir_read(HANDLE handle, u32 idx, lightos_direntry_t* namebuffer, size_t blen)
{
    return syscall_4(SYSID_DIR_READ, handle, idx, (u64)namebuffer, blen);
}

size_t sys_seek(HANDLE handle, u64 offset, u32 type)
{
    return syscall_3(SYSID_SEEK, handle, offset, type);
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

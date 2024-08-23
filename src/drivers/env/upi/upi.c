#include "drivers/env/upi/upi.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "lightos/handle_def.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "sys/types.h"
#include <proc/env.h>
#include <proc/proc.h>
#include <sched/scheduler.h>

static oss_node_t* _upi_pipe_node;
static zone_allocator_t* _upi_pipe_allocator = NULL;
static zone_allocator_t* _upi_listener_allocator = NULL;

/*
 * List to keep a global list of all the pipes and listeners created
 * TODO: Graceful driver shutdown
 */
static upi_pipe_t* _upi_pipe_list;

static inline void __upi_register_pipe(upi_pipe_t* pipe)
{
    pipe->_upi_next = _upi_pipe_list;
    _upi_pipe_list = pipe;
}

static inline void __upi_unregister_pipe(upi_pipe_t* pipe)
{
    upi_pipe_t** walker = &_upi_pipe_list;

    while (*walker && *walker != pipe)
        walker = &(*walker)->_upi_next;

    /* Not found */
    if (!(*walker))
        return;

    /* Do the unlink */
    *walker = pipe->_upi_next;
}

static inline void upi_pipe_link_listener(upi_pipe_t* pipe, upi_listener_t* listener)
{
    listener->next = pipe->listeners;
    pipe->listeners = listener;
}

static inline void upi_pipe_unlink_listener(upi_pipe_t* pipe, upi_listener_t* listener)
{
    upi_listener_t** walker = &pipe->listeners;

    while (*walker && *walker != listener)
        walker = &(*walker)->next;

    /* Not found */
    if (!(*walker))
        return;

    /* Do the unlink */
    *walker = listener->next;
}

/*!
 * @brief: Creates a listener on a UPI pipe
 *
 * This also links the listener into the desired pipe
 *
 * @proc: The process which wishes to listen in on a certain pipe. This process should already have
 * opened the pipe and have a handle to it.
 * @pipe_handle: The handle of @proc to the pipe it wishes to connect to
 */
upi_listener_t* create_upi_listener(proc_t* proc, upi_pipe_t* pipe, HANDLE pipe_handle)
{
    upi_listener_t* ret;

    if (!proc || !pipe)
        return nullptr;

    ret = zalloc_fixed(_upi_listener_allocator);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->pipe_handle = pipe_handle;
    ret->proc = proc;
    ret->ft_idx = pipe->ft_w_idx;

    upi_pipe_link_listener(pipe, ret);

    return ret;
}

/*!
 * @brief: Kills upi listener memory
 *
 * This also unlinks the listener from its pipe after the handle has been yoinked
 */
void destroy_upi_listener(upi_pipe_t* pipe, upi_listener_t* listener)
{
    khandle_t* handle;

    if (!listener || !listener->proc)
        return;

    /* Unlink the listener from it's pipe */
    upi_pipe_unlink_listener(pipe, listener);

    mutex_lock(listener->proc->m_handle_map.lock);

    handle = find_khandle(&listener->proc->m_handle_map, listener->pipe_handle);

    if (!handle)
        goto free_and_exit;

    if (unbind_khandle(&listener->proc->m_handle_map, handle))
        goto free_and_exit;

free_and_exit:
    mutex_unlock(listener->proc->m_handle_map.lock);
    zfree_fixed(_upi_listener_allocator, listener);
}

upi_listener_t* get_upi_listener(upi_pipe_t* pipe, proc_t* proc)
{
    upi_listener_t* ret;

    ret = pipe->listeners;

    /* Scan until we find a listener from @proc */
    while (ret && ret->proc != proc)
        ret = ret->next;

    return ret;
}

/*!
 * @brief: Check if a process can connect to a pipe
 */
bool upi_pipe_can_proc_connect(upi_pipe_t* pipe, proc_t* proc)
{
    if (pipe->max_listeners && pipe->n_listeners == pipe->max_listeners)
        return false;

    /* No duplicates */
    if (get_upi_listener(pipe, proc))
        return false;

    return true;
}

/*!
 * @brief: Actual destructor for a UPI pipe
 */
static void __destroy_upi_pipe(upi_pipe_t* pipe)
{
    upi_listener_t *c_listener, *next_listener;

    ASSERT_MSG(pipe, "__destroy_upi_pipe: Recieved null pipe???");

    /* Unregister the pipe from UPI */
    __upi_unregister_pipe(pipe);

    /* Remove all instances of this pipe from the creator_procs handle map */
    khandle_map_remove(&pipe->creator_proc->m_handle_map, HNDL_TYPE_OSS_OBJ, pipe->obj);

    /* Make sure these fuckers aren't gonna fuck us either */
    for (upi_listener_t* i = pipe->listeners; i != nullptr; i = i->next)
        khandle_map_remove(&i->proc->m_handle_map, HNDL_TYPE_OSS_OBJ, pipe->obj);

    if (pipe->uniform_allocator)
        destroy_zone_allocator(pipe->uniform_allocator, false);

    for (u32 i = 0; i < pipe->ft_capacity; i++)
        /*
         * This function destroys the buffer allocated for this ft if it was
         * a data transaction. Any other type of ft does not need special
         * attention and simply mudering the ft buffer will be enough
         */
        upi_destroy_transaction(pipe, &pipe->ft_buffer[i]);

    /* Deallocate the ft buffer */
    __kmem_kernel_dealloc((vaddr_t)pipe->ft_buffer, sizeof(lightos_pipe_ft_t) * pipe->ft_capacity);

    c_listener = pipe->listeners;

    while (c_listener) {
        next_listener = c_listener->next;

        /* Destroy this listener */
        destroy_upi_listener(pipe, c_listener);

        c_listener = next_listener;
    }

    zfree_fixed(_upi_pipe_allocator, pipe);
}

/*!
 * @brief: Allocate and initialize memory for a UPI pipe
 *
 * Creates an OSS object and registers it to the creator processes environment
 */
upi_pipe_t* create_upi_pipe(proc_t* proc, lightos_pipe_t __user* upipe)
{
    upi_pipe_t* ret;
    oss_node_t* pipe_node;

    if (!upipe)
        return nullptr;

    /* Can't have a uniform pipe without a datasize */
    if (((upipe->flags & LIGHTOS_UPIPE_FLAGS_UNIFORM) == LIGHTOS_UPIPE_FLAGS_UNIFORM) && !upipe->data_size)
        return nullptr;

    ret = zalloc_fixed(_upi_pipe_allocator);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->creator_proc = proc;
    ret->n_listeners = 0;
    ret->max_listeners = upipe->max_listeners;
    ret->flags = upipe->flags;
    ret->obj = create_oss_obj(upipe->name);

    if (!ret->obj)
        goto destroy_and_exit;

    /* Uniform pipes have their own data cache */
    if (upi_pipe_is_uniform(ret)) {
        ret->uniform_allocator = create_zone_allocator(64 * Kib, upipe->data_size, NULL);

        if (!ret->uniform_allocator)
            goto destroy_and_exit;
    }

    ret->ft_capacity = UPI_DEFAULT_FT_CAPACITY;

    /* Try to allocate a buffer */
    if (__kmem_kernel_alloc_range((void**)&ret->ft_buffer, sizeof(lightos_pipe_ft_t) * ret->ft_capacity, KMEM_CUSTOMFLAG_CREATE_USER, KMEM_FLAG_WRITABLE) || !ret->ft_buffer)
        goto destroy_and_exit;

    memset(ret->ft_buffer, 0, sizeof(lightos_pipe_ft_t) * ret->ft_capacity);

    pipe_node = proc->m_env->node;

    /* If we wanted to create a global pipe, try to add it here */
    if ((ret->flags & LIGHTOS_UPIPE_FLAGS_GLOBAL) == LIGHTOS_UPIPE_FLAGS_GLOBAL && _upi_pipe_node)
        pipe_node = _upi_pipe_node;

    /* Finally, add the object to the environment node */
    if (oss_node_add_obj(pipe_node, ret->obj))
        goto destroy_and_exit;

    /* Register the pipe as a child to the object */
    oss_obj_register_child(ret->obj, ret, OSS_OBJ_TYPE_PIPE, __destroy_upi_pipe);

    /* Register the pipe to upi */
    __upi_register_pipe(ret);

    return ret;

destroy_and_exit:

    if (ret->obj)
        destroy_oss_obj(ret->obj);

    if (ret->uniform_allocator)
        destroy_zone_allocator(ret->uniform_allocator, false);

    if (ret->ft_buffer)
        __kmem_kernel_dealloc((vaddr_t)ret->ft_buffer, sizeof(lightos_pipe_t) * ret->ft_capacity);

    zfree_fixed(_upi_pipe_allocator, ret);
    return nullptr;
}

/*!
 * @brief: 'Wrapper' function for the destruction of upi pipes
 */
void destroy_upi_pipe(upi_pipe_t* pipe)
{
    /* Destroying the pipes object will also kill the pipe */
    destroy_oss_obj(pipe->obj);
}

upi_pipe_t* get_upi_pipe(proc_t* proc, HANDLE handle, khandle_t** pkhandle)
{
    oss_obj_t* obj;
    khandle_t* pipe_khandle;

    if (!proc)
        return nullptr;

    pipe_khandle = find_khandle(&proc->m_handle_map, handle);

    /* Invalid handle */
    if (!pipe_khandle || pipe_khandle->type != HNDL_TYPE_OSS_OBJ)
        return nullptr;

    obj = pipe_khandle->reference.oss_obj;

    /* Still an invalid handle */
    if (!obj || obj->type != OSS_OBJ_TYPE_PIPE)
        return nullptr;

    if (pkhandle)
        *pkhandle = pipe_khandle;

    return oss_obj_unwrap(obj, upi_pipe_t);
}

u64 upi_msg(struct aniva_driver* this, driver_control_code_t dcc, void* in_buf, size_t in_bsize, void* out_buf, size_t out_bsize)
{
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check if the buffer is mapped to a valid address */
    if (kmem_validate_ptr(c_proc, (u64)in_buf, in_bsize))
        return DRV_STAT_INVAL;

    switch (dcc) {
    case LIGHTOS_UPI_MSG_CREATE_PIPE:
        if (in_bsize != sizeof(lightos_pipe_t))
            return DRV_STAT_INVAL;

        return upi_create_pipe(c_proc, (lightos_pipe_t*)in_buf);
    case LIGHTOS_UPI_MSG_DESTROY_PIPE:
        if (in_bsize != sizeof(HANDLE))
            return DRV_STAT_INVAL;

        return upi_destroy_pipe(c_proc, *(HANDLE*)in_buf);
    case LIGHTOS_UPI_MSG_CONNECT_PIPE:
        if (in_bsize != sizeof(lightos_pipe_t))
            return DRV_STAT_INVAL;

        return upi_connect_pipe(c_proc, (lightos_pipe_t*)in_buf);
    case LIGHTOS_UPI_MSG_DISCONNECT_PIPE:
        if (in_bsize != sizeof(HANDLE))
            return DRV_STAT_INVAL;

        return upi_disconnect_pipe(c_proc, *(HANDLE*)in_buf);
    case LIGHTOS_UPI_MSG_SEND_TRANSACT:
        if (in_bsize != sizeof(lightos_pipe_ft_t))
            return DRV_STAT_INVAL;

        return upi_send_transact(c_proc, (lightos_pipe_ft_t*)in_buf);
    case LIGHTOS_UPI_MSG_ACCEPT_TRANSACT:
        if (in_bsize != sizeof(lightos_pipe_accept_t))
            return DRV_STAT_INVAL;

        return upi_transact_accept(c_proc, (lightos_pipe_accept_t*)in_buf);
    case LIGHTOS_UPI_MSG_DENY_TRANSACT:
        if (in_bsize != sizeof(HANDLE))
            return DRV_STAT_INVAL;

        return upi_transact_deny(c_proc, *(HANDLE*)in_buf);
    case LIGHTOS_UPI_MSG_PREVIEW_TRANSACT:
        if (in_bsize != sizeof(lightos_pipe_ft_t))
            return DRV_STAT_INVAL;

        return upi_transact_preview(c_proc, (lightos_pipe_ft_t*)in_buf);
    case LIGHTOS_UPI_MSG_DUMP_PIPE:
        if (in_bsize != sizeof(lightos_pipe_dump_t))
            return DRV_STAT_INVAL;

        return upi_dump_pipe(c_proc, (lightos_pipe_dump_t*)in_buf);
    }

    return DRV_STAT_INVAL;
}

int upi_init(drv_manifest_t* this)
{
    oss_node_t* root_node;

    /* Make sure these things are NULL */
    _upi_pipe_node = NULL;
    _upi_pipe_allocator = NULL;
    _upi_listener_allocator = NULL;

    _upi_pipe_list = NULL;

    _upi_pipe_allocator = create_zone_allocator(1 * Mib, sizeof(upi_pipe_t), NULL);

    if (!_upi_pipe_allocator)
        return -KERR_NOMEM;

    _upi_listener_allocator = create_zone_allocator(64 * Kib, sizeof(upi_listener_t), NULL);

    if (!_upi_listener_allocator)
        return -KERR_NOMEM;

    root_node = NULL;

    /* Failed to resolve root, no biggie */
    if (oss_resolve_node("%/Runtime", &root_node) || !root_node)
        return 0;

    /* Try to create the node */
    _upi_pipe_node = create_oss_node("Upi", OSS_OBJ_STORE_NODE, NULL, root_node);

    return 0;
}

int upi_exit()
{
    for (upi_pipe_t* pipe = _upi_pipe_list; pipe != nullptr; pipe = pipe->_upi_next)
        destroy_upi_pipe(pipe);

    if (_upi_pipe_allocator)
        destroy_zone_allocator(_upi_pipe_allocator, false);
    if (_upi_listener_allocator)
        destroy_zone_allocator(_upi_listener_allocator, false);

    /* Destroy the oss node for global pipes */
    destroy_oss_node(_upi_pipe_node);
    return 0;
}

EXPORT_DRIVER(env_driver) = {
    .m_name = LIGHTOS_UPIPE_DRIVERNAME,
    .m_type = DT_SERVICE,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = upi_init,
    .f_exit = upi_exit,
    .f_msg = upi_msg,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};

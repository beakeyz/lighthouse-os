#include "pipe.h"
#include <errno.h>
#include <lightos/driver/drv.h>
#include <stdio.h>
#include <string.h>
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "time.h"
#include "unistd.h"

static HANDLE upi_handle = HNDL_INVAL;

/*!
 * @brief: Checks if the UPI driver is loaded and opens it
 *
 * Should be called every time we call upi functions
 */
static inline int __check_upi()
{
    /* We already have a valid handle to the driver */
    if (handle_verify(upi_handle))
        return 0;

    if (!open_driver(LIGHTOS_UPIPE_DRIVER, HNDL_FLAG_RW, NULL, &upi_handle))
        return -ENODEV;

    return 0;
}

static inline int _lightos_pipe_create(lightos_pipe_t* ppipe)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_CREATE_PIPE, ppipe, sizeof(*ppipe)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_destroy(HANDLE pipe)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_DESTROY_PIPE, &pipe, sizeof(pipe)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_connect(lightos_pipe_t* pipe)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_CONNECT_PIPE, pipe, sizeof(*pipe)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_disconnect(HANDLE pipe)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_DISCONNECT_PIPE, &pipe, sizeof(pipe)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_send(lightos_pipe_ft_t* ft)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_SEND_TRANSACT, ft, sizeof(*ft)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_accept(lightos_pipe_accept_t* accept)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_ACCEPT_TRANSACT, accept, sizeof(*accept)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_deny(HANDLE pipe)
{
    if (__check_upi())
        return -ENODEV;

    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_DENY_TRANSACT, &pipe, sizeof(pipe)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_preview(lightos_pipe_ft_t* ft)
{
    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_PREVIEW_TRANSACT, ft, sizeof(*ft)))
        return 0;

    return -1;
}

static inline int _lightos_pipe_dump(lightos_pipe_dump_t* dump)
{
    if (driver_send_msg(upi_handle, LIGHTOS_UPI_MSG_DUMP_PIPE, dump, sizeof(*dump)))
        return 0;

    return -1;
}

int init_lightos_pipe(lightos_pipe_t* pipe, const char* name, DWORD flags, DWORD max_listeners, DWORD datasize)
{
    memset(pipe, 0, sizeof(*pipe));

    /* Copy the pipe name */
    strncpy((char*)pipe->name, name, sizeof(pipe->name) - 1);
    pipe->flags = flags;
    pipe->max_listeners = max_listeners;
    pipe->data_size = datasize;

    /* The driver will finalise pipe creation and give us a handle */
    return _lightos_pipe_create(pipe);
}

int init_lightos_pipe_uniform(lightos_pipe_t* pipe, const char* name, DWORD flags, DWORD max_listeners, DWORD datasize)
{
    if (!datasize)
        return -EINVAL;

    return init_lightos_pipe(pipe, name, flags | LIGHTOS_UPIPE_FLAGS_UNIFORM, max_listeners, datasize);
}

int destroy_lightos_pipe(lightos_pipe_t* pipe)
{
    return _lightos_pipe_destroy(pipe->pipe);
}

int lightos_pipe_dump(lightos_pipe_t* pipe, lightos_pipe_dump_t* pdump)
{
    if (!pipe || !pdump)
        return -EINVAL;

    /* Tell the driver what pipe we want to dump */
    pdump->pipe = pipe->pipe;

    /* Ask the driver to dump info about the pipe into the dump struct */
    return _lightos_pipe_dump(pdump);
}

int lightos_pipe_connect(lightos_pipe_t* pipe, const char* path)
{
    HANDLE pipe_handle;

    if (!pipe || !path)
        return -EINVAL;

    pipe_handle = open_handle(path, HNDL_TYPE_OSS_OBJ, HNDL_FLAG_RW, HNDL_MODE_NORMAL);

    /* Could not find this pipe */
    if (!handle_verify(pipe_handle))
        return -EPIPE;

    /* Clear pipe */
    memset(pipe, 0, sizeof(*pipe));

    /* Hihi */
    pipe->pipe = pipe_handle;

    /*
     * The driver will copy all the needed information about the pipe
     * into our structure
     */
    return _lightos_pipe_connect(pipe);
}

int lightos_pipe_connect_rel(lightos_pipe_t* pipe, HANDLE rel_handle, const char* name)
{
    HANDLE pipe_handle;

    if (!pipe || !name)
        return -EINVAL;

    pipe_handle = open_handle_rel(rel_handle, name, HNDL_FLAG_RW, NULL);

    /* Could not find this pipe */
    if (!handle_verify(pipe_handle))
        return -EPIPE;

    /* Clear pipe */
    memset(pipe, 0, sizeof(*pipe));

    /* Hihi */
    pipe->pipe = pipe_handle;

    /*
     * The driver will copy all the needed information about the pipe
     * into our structure
     */
    return _lightos_pipe_connect(pipe);
}

int lightos_pipe_disconnect(lightos_pipe_t* pipe)
{
    return _lightos_pipe_disconnect(pipe->pipe);
}

int lightos_pipe_send(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, int type, void* pdata, size_t size)
{
    int error;
    lightos_pipe_ft_t ft;

    ft.transaction = (lightos_pipe_transaction_t) {
        .transaction_type = type,
        .data_size = size,
        .deny_count = 0,
        .accept_count = 0,
    };
    ft.pipe_handle = pipe->pipe;

    switch (type) {
        case LIGHTOS_PIPE_TRANSACT_TYPE_DATA:
            ft.payload.data = pdata;
            break;
        case LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL:
            ft.payload.signal = (int)(uintptr_t)pdata;
            ft.transaction.data_size = sizeof(int);
            break;
        case LIGHTOS_PIPE_TRANSACT_TYPE_HANDLE:
            ft.payload.handle = (HANDLE)(uintptr_t)pdata;
            ft.transaction.data_size = sizeof(HANDLE);
            break;
    }

    error =  _lightos_pipe_send(&ft);

    /* Export the transaction */
    *p_transaction = ft.transaction;

    return error;
}

int lightos_pipe_send_data(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, void* pdata, size_t size)
{
    return lightos_pipe_send(pipe, p_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_DATA, pdata, size);
}

int lightos_pipe_send_signal(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, int signal)
{
    return lightos_pipe_send(pipe, p_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL, (void*)(uintptr_t)signal, sizeof(signal));
}

int lightos_pipe_send_handle(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, HANDLE handle)
{
    return lightos_pipe_send(pipe, p_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_HANDLE, (void*)(uintptr_t)handle, sizeof(handle));
}

int lightos_pipe_preview(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction)
{
    int error;
    lightos_pipe_ft_t ft = { 0 };

    if (!pipe || !p_transaction)
        return -EINVAL;

    /* Preview expects the ft to have the handle variable set */
    ft.pipe_handle = pipe->pipe;

    error = _lightos_pipe_preview(&ft);

    *p_transaction = ft.transaction;

    return error;
}

int lightos_pipe_await_transaction(lightos_pipe_t* pipe, lightos_pipe_transaction_t* ptransaction)
{
    int error;
    lightos_pipe_transaction_t transact;

    do {
        error = lightos_pipe_preview(pipe, &transact);

        if (error)
            break;

        // TODO: Yield
        usleep(1000);
    } while (!lightos_transaction_is_valid(&transact));

    if (ptransaction)
        *ptransaction = transact;

    return error;
}

int lightos_pipe_accept(lightos_pipe_t* pipe, void* pdata, size_t psize)
{
    lightos_pipe_accept_t accept = { 0 };

    accept.pipe_handle = pipe->pipe;
    accept.buffer = pdata;
    accept.buffer_size = psize;

    return _lightos_pipe_accept(&accept);
}

int lightos_pipe_deny(lightos_pipe_t* pipe)
{
    return _lightos_pipe_deny(pipe->pipe);
}

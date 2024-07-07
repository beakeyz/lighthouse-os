#include "dev/driver.h"
#include "lightos/handle_def.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "upi.h"

/*!
 * @brief: Create a user pipe and register it
 */
u64 upi_create_pipe(proc_t* proc, lightos_pipe_t* upipe)
{
    upi_pipe_t* pipe;
    khandle_t pipe_handle = { 0 };
    khandle_type_t type = HNDL_TYPE_OSS_OBJ;

    pipe = create_upi_pipe(proc, upipe);

    if (!pipe)
        return DRV_STAT_INVAL;

    /* Initialize the handle of this pipe */
    init_khandle(&pipe_handle, &type, pipe->obj);

    /* Bind the handle to the processes handle map */
    if (bind_khandle(&proc->m_handle_map, &pipe_handle, (u32*)&upipe->pipe)) {
        destroy_upi_pipe(pipe);
        return DRV_STAT_INVAL;
    }

    return 0;
}

/*!
 * @brief: Destroys an entire pipeline
 *
 * Called by the process which created the pipe when it's done sending/recieving data through it.
 * Processes which are listening to this pipe will have their handles revoked
 */
u64 upi_destroy_pipe(proc_t* proc, HANDLE pipe_handle)
{
    upi_pipe_t* pipe;
    khandle_t* pipe_khandle;

    /* Grab the pipe from the handle */
    pipe = get_upi_pipe(proc, pipe_handle, &pipe_khandle);

    if (!pipe)
        return DRV_STAT_INVAL;

    /* Only the creator process can kill this pipe */
    if (pipe->creator_proc != proc)
        return DRV_STAT_INVAL;

    /* completely fucking murder this pipe and its connections */
    destroy_upi_pipe(pipe);
    return 0;
}

/*!
 * @brief: Connect a process to a userpipe
 *
 * The upipe struct passed by the process should already contain a handle from the requesting
 * process to the pipe. We use this to link to the listener
 */
u64 upi_connect_pipe(proc_t* proc, lightos_pipe_t* upipe)
{
    upi_pipe_t* pipe;

    pipe = get_upi_pipe(proc, upipe->pipe, NULL);

    /* Invalid handle */
    if (!pipe)
        return DRV_STAT_INVAL;

    /* Proc can not connect to this pipe, yikes */
    if (!upi_pipe_can_proc_connect(pipe, proc))
        return DRV_STAT_INVAL;

    /* We may connect, do da thing */
    if (!create_upi_listener(proc, pipe, upipe->pipe))
        return DRV_STAT_INVAL;

    /* Reference the pipe object */
    oss_obj_ref(pipe->obj);

    upipe->max_listeners= pipe->max_listeners;
    upipe->flags = pipe->flags;
    upipe->data_size = upi_pipe_get_uniform_datasize(pipe);

    /* Copy over the name */
    strncpy((char*)upipe->name, pipe->obj->name, sizeof(upipe->name));

    return 0;
}

u64 upi_disconnect_pipe(proc_t* proc, HANDLE pipe_handle)
{
    upi_pipe_t* pipe;
    upi_listener_t* listener;

    pipe = get_upi_pipe(proc, pipe_handle, NULL);

    if (!pipe)
        return DRV_STAT_INVAL;

    listener = get_upi_listener(pipe, proc);

    if (!listener)
        return DRV_STAT_INVAL;

    /* Murder the listener */
    destroy_upi_listener(pipe, listener);
    return 0;
}

/*!
 * @brief: Sends a transact into the specified pipe
 *
 * In the case of data transactions, the data of the transaction gets completely yoinked by the kernel
 */
u64 upi_send_transact(proc_t* proc, lightos_pipe_ft_t* ft)
{
    upi_pipe_t* pipe;

    if (!ft)
        return DRV_STAT_INVAL;

    /* Grab the pipe handle */
    pipe = get_upi_pipe(proc, ft->pipe_handle, NULL);

    if (!pipe)
        return DRV_STAT_INVAL;

    /* Handle the signal transaction if it needs imediate attention */
    if (ft->transaction.transaction_type == LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL)
        upi_maybe_handle_signal_transact(proc, ft);

    /* Add the transaction to the pipes buffer */
    return upi_pipe_add_transaction(pipe, ft);
}

/*!
 * @brief: Grab the transaction info for the current transaction on a connection
 */
u64 upi_transact_preview(proc_t* proc, lightos_pipe_ft_t* tranact)
{
    upi_pipe_t* pipe;
    upi_listener_t* listener;
    lightos_pipe_ft_t* c_ft;

    pipe = get_upi_pipe(proc, tranact->pipe_handle, NULL);

    if (!pipe)
        return DRV_STAT_INVAL;

    listener = get_upi_listener(pipe, proc);

    if (!listener)
        return DRV_STAT_INVAL;

    /* Get the current ft for this connection */
    c_ft = upi_listener_get_ft(pipe, listener, NULL);

    if (!c_ft) {
        /* Fuck, we need a new transaction */
        upi_pipe_next_transaction(pipe, &listener->ft_idx);

        c_ft = upi_listener_get_ft(pipe, listener, NULL);
    }

    /* Copy the transaction into the ft struct */
    memcpy(&tranact->transaction, &c_ft->transaction, sizeof(lightos_pipe_transaction_t));

    return 0;
}


u64 upi_transact_accept(proc_t* proc, lightos_pipe_accept_t* accept)
{
    size_t bsize;
    upi_pipe_t* pipe;
    upi_listener_t* listener;
    u32 c_ft_idx;
    lightos_pipe_ft_t* c_ft;

    pipe = get_upi_pipe(proc, accept->pipe_handle, NULL);

    if (!pipe)
        return DRV_STAT_INVAL;

    listener = get_upi_listener(pipe, proc);

    if (!listener)
        return DRV_STAT_INVAL;

    /* Grab the current ft for this connection */
    c_ft = upi_listener_get_ft(pipe, listener, &c_ft_idx);

    /* Increment the current transfer index on the listener */
    (void)upi_pipe_next_transaction(pipe, &listener->ft_idx);

    /* Empty transaction, don't do anything */
    if (!c_ft || c_ft->transaction.transaction_type == LIGHTOS_PIPE_TRANSACT_TYPE_NONE)
        return DRV_STAT_INVAL;

    /* Increment the data on this transaction */
    c_ft->transaction.accept_count++;

    /* Increment the data on this connection */
    listener->n_accepted_transacts++;
    listener->n_transacts++;

    /* Copy the data in the ft into the accept buffer */
    switch (c_ft->transaction.transaction_type) {
        case LIGHTOS_PIPE_TRANSACT_TYPE_DATA:
            bsize = accept->buffer_size;

            /* If the acceptor has a bigger buffer than the transactions datasize, use the datasize */
            if (bsize > c_ft->transaction.data_size)
                bsize = c_ft->transaction.data_size;

            /* Copy the data */
            memcpy(accept->buffer, c_ft->payload.data, bsize);
            break;
        case LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL:
            bsize = accept->buffer_size;

            if (bsize > sizeof(c_ft->payload.signal))
                bsize = sizeof(c_ft->payload.signal);
            
            memcpy(accept->buffer, &c_ft->payload.signal, bsize);
            break;

        case LIGHTOS_PIPE_TRANSACT_TYPE_HANDLE:
            bsize = accept->buffer_size;

            if (bsize > sizeof(c_ft->payload.handle))
                bsize = sizeof(c_ft->payload.handle);
            
            memcpy(accept->buffer, &c_ft->payload.handle, bsize);
            break;

    }

    /* Check if the pipe can throw this ft out */
    return upi_pipe_check_transaction(pipe, c_ft_idx);
}

u64 upi_transact_deny(proc_t* proc, HANDLE handle)
{
    upi_pipe_t* pipe;
    upi_listener_t* listener;
    lightos_pipe_ft_t* c_ft;
    u32 c_ft_idx;

    pipe = get_upi_pipe(proc, handle, NULL);

    if (!pipe)
        return DRV_STAT_INVAL;

    listener = get_upi_listener(pipe, proc);

    if (!listener)
        return DRV_STAT_INVAL;

    c_ft = upi_listener_get_ft(pipe, listener, &c_ft_idx);

    /* Increment the current transfer index on the listener */
    (void)upi_pipe_next_transaction(pipe, &listener->ft_idx);

    /* Empty transaction, don't do anything */
    if (!c_ft || c_ft->transaction.transaction_type == LIGHTOS_PIPE_TRANSACT_TYPE_NONE)
        return DRV_STAT_INVAL;

    /* Increment the data on this transaction */
    c_ft->transaction.deny_count++;

    /* Increment the data on this connection */
    listener->n_transacts++;

    /* Maybe remove this transaction if its done */
    return upi_pipe_check_transaction(pipe, c_ft_idx);
}

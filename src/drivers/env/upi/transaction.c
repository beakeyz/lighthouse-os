#include "dev/driver.h"
#include "libk/flow/error.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include "upi.h"

static inline bool _upi_pipe_has_fragmentation(upi_pipe_t* pipe)
{
    return (pipe->ft_w_idx != pipe->next_free_ft);
}

static inline void* _upi_pipe_allocate(upi_pipe_t* pipe, size_t size)
{
    if (upi_pipe_is_uniform(pipe))
        return zalloc_fixed(pipe->uniform_allocator);

    return kmalloc(size);
}

static inline void _upi_pipe_deallocate(upi_pipe_t* pipe, void* addr)
{
    if (upi_pipe_is_uniform(pipe))
        return zfree_fixed(pipe->uniform_allocator, addr);

    kfree(addr);
}

void upi_destroy_transaction(upi_pipe_t* pipe, lightos_pipe_ft_t* ft)
{
    if (ft->transaction.transaction_type != LIGHTOS_PIPE_TRANSACT_TYPE_DATA)
        return;

    /* Deallocate the transactions buffer */
    _upi_pipe_deallocate(pipe, ft->payload.data);
}

/*!
 * @brief: Add a ft at @idx withtout question
 */
static u64 _upi_pipe_do_ft_add(upi_pipe_t* pipe, lightos_pipe_ft_t __user* ft, u32 idx)
{
    u64 data_size;
    lightos_pipe_ft_t* target;

    /* Invalid index */
    if (idx >= pipe->ft_capacity)
        return DRV_STAT_INVAL;

    target = &pipe->ft_buffer[idx];

    ASSERT_MSG(target->transaction.transaction_type == 0, "_upi_pipe_do_regular_ft_add: Transaction slot was not free!");

    /* Copy the transaction header */
    memcpy(&target->transaction, &ft->transaction, sizeof(lightos_pipe_transaction_t));
    target->pipe_handle = ft->pipe_handle;
    target->next_free = 0;

    data_size = target->transaction.data_size;

    /* If the transaction gives us a bigger buffer than this uniform pipe can handle, cut it down */
    if (upi_pipe_is_uniform(pipe) && pipe->uniform_allocator && data_size > pipe->uniform_allocator->m_entry_size)
        data_size = pipe->uniform_allocator->m_entry_size;

    switch (target->transaction.transaction_type) {
        case LIGHTOS_PIPE_TRANSACT_TYPE_HANDLE:
            target->payload.handle = ft->payload.handle;
            break;
        case LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL:
            target->payload.signal = ft->payload.signal;
            break;
        case LIGHTOS_PIPE_TRANSACT_TYPE_DATA:
            /* Allocate the buffer */
            target->payload.data = _upi_pipe_allocate(pipe, data_size);

            if (!target->payload.data)
                goto clear_and_exit_error;

            /* Clear any unwanted data */
            memset(target->payload.data, 0, data_size);

            /* Copy the entire buffer to us */
            memcpy(target->payload.data, ft->payload.data, data_size);
            break;
    }

    pipe->n_total_transacts++;
    pipe->n_ft++;
    return 0;

clear_and_exit_error:
    memset(target, 0, sizeof(*target));
    return DRV_STAT_INVAL;
}

static inline u64 _upi_pipe_do_regular_ft_add(upi_pipe_t* pipe, lightos_pipe_ft_t __user* ft)
{
    u64 res = _upi_pipe_do_ft_add(pipe, ft, pipe->ft_w_idx);

    /* Make sure to return errors */
    if (res)
        return res;

    do {
        pipe->ft_w_idx = (pipe->ft_w_idx + 1) % pipe->ft_capacity;
        pipe->next_free_ft = pipe->ft_w_idx;
        /*
         * If this add fills up the ft buffer until the max, don't try to find a new free ft slot.
         * Simply reset both ft_w_idx and next_free_ft and return success. At this point we'll most
         * likely only be doing fragmented adds
         */
    } while (pipe->n_ft != pipe->ft_capacity && pipe->ft_buffer[pipe->ft_w_idx].transaction.transaction_type != LIGHTOS_PIPE_TRANSACT_TYPE_NONE);

    return res;
}

static inline void _upi_pipe_switch_free_indecies(u32* next_free_a, u32* next_free_b)
{
    *next_free_a = (*next_free_a ^ *next_free_b);
    *next_free_b = (*next_free_b ^ *next_free_a);
    *next_free_a = (*next_free_a ^ *next_free_b);
}

/*!
 * @brief: Adds a ft to the pipe when its buffer is in a fragmented state
 *
 * When a transaction is removed which causes fragmentation, the slot will be reset and transaction_type will be 
 * set to NONE, as usual, but the next_free will be set. We can then follow a (kind of) linked list until we return
 * back to the index of ft_w_idx.
 */
static inline u64 _upi_pipe_do_fragmented_ft_add(upi_pipe_t* pipe, lightos_pipe_ft_t __user* ft)
{
    lightos_pipe_ft_t* next_free_ft;

    /* Read the next free ft */
    next_free_ft = &pipe->ft_buffer[pipe->next_free_ft];

    /* Make this free entry point to itself */
    _upi_pipe_switch_free_indecies(&pipe->next_free_ft, &next_free_ft->next_free);

    /* Put the new ft in the spot of the next free ft */
    return _upi_pipe_do_ft_add(pipe, ft, next_free_ft->next_free);
}

/*!
 * @brief: Adds a transaction to the pipes ft buffer
 *
 * Checks if ft_w_idx and next_free_ft are equal. In this case there is no fragmentation, and we can
 * just stick the new ft at the end of the buffer. If the two are not equal, there is fragmentation.
 *
 */
u64 upi_pipe_add_transaction(upi_pipe_t* pipe, lightos_pipe_ft_t __user* ft)
{
    if (pipe->n_ft == pipe->ft_capacity)
        return DRV_STAT_FULL;

    /* No fragmentation, just stick it at the end */
    if (!_upi_pipe_has_fragmentation(pipe))
        return _upi_pipe_do_regular_ft_add(pipe, ft);

    return _upi_pipe_do_fragmented_ft_add(pipe, ft);
}

/*!
 * @brief: Remove a transaction from the pipe
 *
 * Updates the pipes next_free_ft variable to the index of this ft
 */
u64 upi_pipe_remove_transaction(upi_pipe_t* pipe, u32 ft_idx)
{
    lightos_pipe_ft_t* ft;

    /* Can't do shit */
    if (!pipe->n_ft)
        return DRV_STAT_NOT_FOUND;

    if (ft_idx >= pipe->ft_capacity)
        return DRV_STAT_OUT_OF_RANGE;

    ft = &pipe->ft_buffer[ft_idx];

    /* Murder the transaction memory */
    upi_destroy_transaction(pipe, ft);

    /* Clear the memory */
    memset(ft, 0, sizeof(*ft));

    /* Mark the next free ft */
    ft->next_free = pipe->next_free_ft;
    /* This ft is now the first next free ft */
    pipe->next_free_ft = ft_idx;
    pipe->n_ft--;

    return 0;
}

/*!
 * @brief: Checks if the transaction at @idx has been completed and removes it if it has
 *
 * A transaction has been completed when every listener on the pipe has said to have handled the transaction, 
 * of if a certain threshold, set by the pipe, has been reached.
 *
 * NOTE: Caller should check if idx is in the range of the ft buffer
 */
u64 upi_pipe_check_transaction(upi_pipe_t* pipe, u32 idx)
{
    u32 threshold;
    lightos_pipe_ft_t* ft;

    ft = &pipe->ft_buffer[idx];
    threshold = pipe->n_listeners;

    /* If the transaction has not been fully completed yet, just leave it be */
    if (lightos_pipe_transact_get_n_handle(&ft->transaction) < threshold)
        return 0;

    return upi_pipe_remove_transaction(pipe, idx);
}

/*!
 * @brief: Get the next active transaction on the pipe
 */
u64 upi_pipe_next_transaction(upi_pipe_t* pipe, u32* p_idx)
{
    u32 prev_idx;
    lightos_pipe_ft_t* c_ft;

    if (!p_idx)
        return DRV_STAT_INVAL;

    prev_idx = *p_idx;
    *p_idx = pipe->next_free_ft;

    /* No transactions, just return */
    if (!pipe->n_ft)
        return 0;

    /* Loop until we find an active transaction */
    for (u32 i = 1; i < pipe->ft_capacity; i++) {
        c_ft = &pipe->ft_buffer[(prev_idx + i) % pipe->ft_capacity];

        if (!c_ft->transaction.transaction_type)
            continue;

        *p_idx = (prev_idx + i) % pipe->ft_capacity;
        return 0;
    }

    return DRV_STAT_NOT_FOUND;
}

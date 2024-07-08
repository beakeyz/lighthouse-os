#ifndef __ANIVA_DRIVERS_UPI_PRIVATE__
#define __ANIVA_DRIVERS_UPI_PRIVATE__

#include "libk/stddef.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "mem/zalloc/zalloc.h"
#include "proc/handle.h"
#include <proc/proc.h>
#include <oss/obj.h>

#define UPI_DEFAULT_FT_CAPACITY 32

typedef struct upi_listener {
    /* Process which is listening */
    proc_t* proc;
    /*
     * Index into the pipes ft buffer, pointing to the current transaction this
     * listener is dealing with 
     */
    u32 ft_idx;
    u32 n_transacts;
    u32 n_accepted_transacts;
    /* The handle of @proc to the pipe this listener is connected to */
    HANDLE pipe_handle;

    /* Link listeners in a linear fashion */
    struct upi_listener* next;
} upi_listener_t;

/*!
 * @brief: Kernel-side pipe structure
 *
 * The kernel driver is responsible for acting as a sort of middleman in charge of every user pipe 
 * that is created (NOTE: This does make this the most likely performance bottleneck in the future).
 * It keeps track of the transactions present in the pipe and the listeners on the pipe
 */
typedef struct upi_pipe {
    /* The process 'in charge' of this pipe */
    proc_t* creator_proc;
    /* The oss object inside the environment of the creator_proc representing the pipe */
    oss_obj_t* obj;
    /* Flags for this pipe */
    u32 flags;

    /* Total number of accepted transactions that have ran through this pipe */
    u64 n_total_accept;
    /* Total number of denied transactions that have ran through this pipe */
    u64 n_total_deny;
    /* Total number of transactions that have ran throught this pipe */
    u64 n_total_transacts;

    /* Index where new packets will be put */
    u32 ft_w_idx;
    /* Number of fts present in the buffer */
    u32 n_ft;
    /*
     * Set when a removal of an ft would cause fragmentation 
     * inside the ft buffer, otherwise equal to ft_w_idx
     */
    u32 next_free_ft;
    /* Capacity of the ft buffer */
    u32 ft_capacity;
    /* Ringbuffer for the ft structs */
    lightos_pipe_ft_t* ft_buffer;

    /* Zone allocator for uniform pipes */
    zone_allocator_t* uniform_allocator;

    u32 max_listeners;
    u32 n_listeners;
    /* Linked list of all listeners */
    upi_listener_t* listeners;
} upi_pipe_t;

static inline bool upi_pipe_is_uniform(upi_pipe_t* pipe)
{
    return ((pipe->flags & LIGHTOS_UPIPE_FLAGS_UNIFORM) == LIGHTOS_UPIPE_FLAGS_UNIFORM);
}

/*!
 * @brief: Grab the internal data size of an uniform pipe
 *
 * NOTE: The zone allocator is also used to store fts, together with the data in blocks
 */
static inline u32 upi_pipe_get_uniform_datasize(upi_pipe_t* pipe)
{
    if (upi_pipe_is_uniform(pipe))
        return 0;

    return pipe->uniform_allocator->m_entry_size - sizeof(lightos_pipe_ft_t);
}

extern u64 upi_create_pipe(proc_t* proc, lightos_pipe_t* upipe);
extern u64 upi_destroy_pipe(proc_t* proc, HANDLE pipe_handle);
extern u64 upi_connect_pipe(proc_t* proc, lightos_pipe_t* upipe);
extern u64 upi_disconnect_pipe(proc_t* proc, HANDLE pipe_handle);
extern u64 upi_send_transact(proc_t* proc, lightos_pipe_ft_t* ft);
extern u64 upi_transact_preview(proc_t* proc, lightos_pipe_ft_t* tranact);
extern u64 upi_transact_accept(proc_t* proc, lightos_pipe_accept_t* accept);
extern u64 upi_transact_deny(proc_t* proc, HANDLE handle);
extern u64 upi_dump_pipe(proc_t* proc, lightos_pipe_dump_t* dump);

extern u64 upi_pipe_add_transaction(upi_pipe_t* pipe, lightos_pipe_ft_t* ft);
extern u64 upi_pipe_check_transaction(upi_pipe_t* pipe, u32 idx);
extern u64 upi_pipe_next_transaction(upi_pipe_t* pipe, u32* p_idx);

int upi_maybe_handle_signal_transact(proc_t* calling_proc, lightos_pipe_ft_t* ft);

upi_pipe_t* create_upi_pipe(proc_t* proc, lightos_pipe_t* upipe);
void destroy_upi_pipe(upi_pipe_t* pipe);

upi_pipe_t* get_upi_pipe(proc_t* proc, HANDLE handle, khandle_t** pkhandle);
bool upi_pipe_can_proc_connect(upi_pipe_t* pipe, proc_t* proc);

upi_listener_t* create_upi_listener(proc_t* proc, upi_pipe_t* pipe, HANDLE pipe_handle);
void destroy_upi_listener(upi_pipe_t* pipe, upi_listener_t* listener);


upi_listener_t* get_upi_listener(upi_pipe_t* pipe, proc_t* proc);

static inline lightos_pipe_ft_t* upi_listener_get_ft(upi_pipe_t* pipe, upi_listener_t* listener, u32* pidx)
{
    lightos_pipe_ft_t* ft;

    if (listener->ft_idx >= pipe->ft_capacity)
        return nullptr;

    ft = &pipe->ft_buffer[listener->ft_idx];

    /* Export the index */
    if (pidx)
        *pidx = listener->ft_idx;
    return ft;
}

static inline u32 upi_listener_get_n_denied_transact(upi_listener_t* listener)
{
    return listener->n_transacts - listener->n_accepted_transacts;
}

static inline u32 upi_listener_get_n_accepted_transact(upi_listener_t* listener)
{
    return listener->n_accepted_transacts;
}

#endif // !__ANIVA_DRIVERS_UPI_PRIVATE__

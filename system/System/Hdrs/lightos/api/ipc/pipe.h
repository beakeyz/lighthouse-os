#ifndef __LIGHTOS_LIBC_PIPE_SHARED__
#define __LIGHTOS_LIBC_PIPE_SHARED__

#include "lightos/api/handle.h"
#define LIGHTOS_UPIPE_DRIVERNAME "upi"
#define LIGHTOS_UPIPE_DRIVER "service/" LIGHTOS_UPIPE_DRIVERNAME

#define LIGHTOS_UPIPE_FLAGS_FULLDUPLEX 0x00000001
#define LIGHTOS_UPIPE_FLAGS_UNIFORM 0x00000002
#define LIGHTOS_UPIPE_FLAGS_GLOBAL 0x00000004

/*
 * full/signle duplex IPC struct
 *
 * Processes can send transactions over the pipe and the other processes on the pipe
 * can choose to accept or deny the transactions.
 *
 * Pipes are represented in the OSS by objects inside processes environments. These pipes
 * are managed by the other/upi (user pipe) driver, which needs to be loaded in order to have
 * communication
 *
 * Communication through pipes
 */
typedef struct lightos_pipe {
    /* Pipes should have short names */
    const char name[32];
    /*
     * Handle to the pipe object. Set by the upi driver when
     * a pipe is created or set by us when we want to connect to a pipe
     */
    HANDLE pipe;
    /* Flags for this pipe */
    unsigned int flags;
    /* When this is a uniform pipe, this is set. Otherwise zero */
    unsigned int data_size;
    /* The maximum number of listeners this pipe may have. Zero for infinite */
    unsigned int max_listeners;
} lightos_pipe_t;

/*
 * Info dump of a certain pipe
 */
typedef struct lightos_pipe_dump {
    HANDLE pipe;
    /* The number of active transactions currently in the pipe */
    unsigned int n_cur_transact;
    /* The number of current connections on the pipe */
    unsigned int n_connection;
    /*
     * Data throughput rate from 0% to 100%, where a higher number means more accepted transactions
     * per single transaction. i.e. averate 'acceptance rate' of all transaction over all connections
     *
     * Calculation: sum((n_accept(i) / n_transact(i)) * 100, i -> n_connection) / n_connection
     */
    unsigned int acceptance_rate;
    /* The number of accepted transactions */
    unsigned long long n_accept;
    /* The number of denied transactions */
    unsigned long long n_deny;
} lightos_pipe_dump_t;

static inline unsigned long long lightos_pipe_n_total_transact(lightos_pipe_dump_t* dump)
{
    return (dump->n_deny + dump->n_accept);
}

enum LIGHTOS_PIPE_TRANSACT_TYPE {
    /* Placeholder for empty transaction structs */
    LIGHTOS_PIPE_TRANSACT_TYPE_NONE = 0,
    /* Simple data transaction, listeners on the pipe can recieve pure data */
    LIGHTOS_PIPE_TRANSACT_TYPE_DATA,
    /* System Signal. The driver either intercepts the transaction or it can choose to let
     * listeners on the pipe handle the signal, based on the type of signal sent */
    LIGHTOS_PIPE_TRANSACT_TYPE_SIGNAL,
    /* Process handle. Listeners on the pipe can recieve a handle from another process,
     * including the object the handle is attached to */
    LIGHTOS_PIPE_TRANSACT_TYPE_HANDLE,
};

typedef struct lightos_pipe_transaction {
    int transaction_type;
    unsigned int accept_count;
    unsigned int deny_count;
    unsigned int data_size;
} lightos_pipe_transaction_t;

static inline unsigned int lightos_pipe_transact_get_n_handle(lightos_pipe_transaction_t* transact)
{
    return (transact->accept_count + transact->deny_count);
}

static inline unsigned char lightos_transaction_is_valid(lightos_pipe_transaction_t* transact)
{
    return (transact && transact->data_size && transact->transaction_type != LIGHTOS_PIPE_TRANSACT_TYPE_NONE);
}

#define LIGHTOS_UPI_MSG_CREATE_PIPE 0
#define LIGHTOS_UPI_MSG_DESTROY_PIPE 1
#define LIGHTOS_UPI_MSG_CONNECT_PIPE 2
#define LIGHTOS_UPI_MSG_DISCONNECT_PIPE 3
#define LIGHTOS_UPI_MSG_SEND_TRANSACT 4
#define LIGHTOS_UPI_MSG_ACCEPT_TRANSACT 5
#define LIGHTOS_UPI_MSG_DENY_TRANSACT 6
#define LIGHTOS_UPI_MSG_PREVIEW_TRANSACT 7
#define LIGHTOS_UPI_MSG_DUMP_PIPE 8

/*
 * Sent to the driver when a transaction is sent
 */
typedef struct lightos_pipe_full_transaction {
    lightos_pipe_transaction_t transaction;
    HANDLE pipe_handle;
    unsigned int next_free;
    union {
        void* data;
        HANDLE handle;
        unsigned int signal;
    } payload;
} lightos_pipe_full_transaction_t, lightos_pipe_ft_t;

/*
 * Sent to the driver when a process wants to accept a transaction from a pipe
 */
typedef struct lightos_pipe_accept {
    HANDLE pipe_handle;
    unsigned int buffer_size;
    void* buffer;
} lightos_pipe_accept_t;

#endif // !__LIGHTOS_LIBC_PIPE_SHARED__

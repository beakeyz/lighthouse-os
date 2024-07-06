#ifndef __LIGHTOS_LIBC_PIPE_SHARED__
#define __LIGHTOS_LIBC_PIPE_SHARED__

#define LIGHTOS_UPIPE_DRIVERNAME "upi"
#define LIGHTOS_UPIPE_DRIVER "service/" LIGHTOS_UPIPE_DRIVERNAME

enum LIGHTOS_PIPE_MODE {
    LIGHTOS_PIPE_MODE_SINGLE,
    LIGHTOS_PIPE_MODE_MULTI,
};

enum LIGHTOS_PIPE_TRANSACT_TYPE {
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

#define LIGHTOS_UPI_MSG_CREATE_PIPE 0
#define LIGHTOS_UPI_MSG_DESTROY_PIPE 1
#define LIGHTOS_UPI_MSG_CONNECT_PIPE 2
#define LIGHTOS_UPI_MSG_SEND_TRANSACT 3
#define LIGHTOS_UPI_MSG_ACCEPT_TRANSACT 4
#define LIGHTOS_UPI_MSG_DENY_TRANSACT 5
#define LIGHTOS_UPI_MSG_PREVIEW_TRANSACT 6

#endif // !__LIGHTOS_LIBC_PIPE_SHARED__

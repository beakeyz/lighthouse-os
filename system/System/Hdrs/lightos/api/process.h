#ifndef __LIGHTOS_PROC_SHARED__
#define __LIGHTOS_PROC_SHARED__

#include <lightos/types.h>
#include <sys/types.h>

/*
 * Either forces process creation or destruction
 * Simply bypasses some safety checks xD
 */
#define LIGHTOS_PROC_FLAG_FORCE 0x00000001

/* This process provides a service */
#define PF_SERVICE 0x00000001
/* Process is from the kernel (spooky) */
#define PF_KERNEL 0x00000002
/* This process comes from a driver */
#define PF_DRIVER 0x00000004
/* This process is idle and waiting to be scheduled */
#define PF_IDLE 0x00000008
/* This process is done and it's threads shouldn't be scheduled anymore */
#define PF_FINISHED 0x00000010
/* Current thread should block until this process is done */
#define PF_SYNC 0x00000020

typedef struct proc_exitvec {
    u32 nr_exit_fn;
    u32 res;
    FuncPtr exit_vec[];
} proc_exitvec_t;

#endif // !__LIGHTOS_PROC_SHARED__

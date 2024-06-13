#ifndef __ANIVA_PROC_CORE__
#define __ANIVA_PROC_CORE__

#include "libk/flow/error.h"

#define DEFAULT_STACK_SIZE (64 * Kib)
#define DEFAULT_THREAD_MAX_TICKS (8)

#define PROC_DEFAULT_MAX_THREADS (16)
#define PROC_CORE_PROCESS_NAME "not_the_kernel"
#define PROC_MAX_TICKS (20)

#define PROC_SOFTMAX (0x1000)

/* The 32-bit limit for proc ids */
#define PROC_HARDMAX (0xFFFFFFFF)

#define THREAD_HARDMAX (0xFFFFFFFF)

#define MIN_SOCKET_BUFFER_SIZE (0)
// FIXME: should this be the max size?
#define SOCKET_DEFAULT_SOCKET_BUFFER_SIZE (4096)
#define SOCKET_DEFAULT_MAXIMUM_SOCKET_COUNT (128)
#define SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT (64)

extern char thread_entry_stub[];
extern char thread_entry_stub_end[];

struct proc;
struct penv;
struct thread;
struct drv_manifest;
struct user_profile;

enum SCHEDULER_PRIORITY;

typedef enum THREAD_STATE {
    INVALID = 0, // not initialized
    RUNNING, // executing
    RUNNABLE, // can be executed by the scheduler
    NO_CONTEXT, // Runnable, but needs to recieve a context
    DYING, // Waiting to be cleaned up
    DEAD, // Gone from the scheduler, in reaper queue
    STOPPED, // stopped by the scheduler, for whatever reason. waiting for reschedule
    BLOCKED, // performing blocking operation
    SLEEPING, // waiting for anything to happen (i.e. signals, data)
} THREAD_STATE_t;

typedef int thread_id_t;

ANIVA_STATUS init_proc_core();

struct thread* allocate_thread();
void deallocate_thread(struct thread* thread);

/*
 * Spawn a thread for the current running process. Does not
 * care about if we are in usermode or kernelland
 */
struct thread* spawn_thread(char name[32], FuncPtr entry, uint64_t arg0);

struct proc* find_proc(const char* path);

bool foreach_proc(bool (*f_callback)(struct proc*));
uint32_t get_proc_count();

struct thread* find_thread(struct proc* proc, thread_id_t tid);

kerror_t proc_register(struct proc* proc, struct user_profile* profile);
kerror_t proc_unregister(struct proc* proc);

bool current_proc_is_kernel();

#endif //__LIGHTHOUSE_OS_CORE__

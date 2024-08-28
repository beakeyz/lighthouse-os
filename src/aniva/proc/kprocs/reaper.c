#include "reaper.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sync/mutex.h"

static thread_t* __reaper_thread;
static uint32_t __reaper_port;
static mutex_t* __reaper_process_lock;
static mutex_t* __reaper_thread_lock;
static queue_t* __reaper_process_queue;
static queue_t* __reaper_thread_queue;

static inline proc_t* _reaper_get_proc()
{
    proc_t* p;

    mutex_lock(__reaper_process_lock);

    p = queue_dequeue(__reaper_process_queue);

    mutex_unlock(__reaper_process_lock);

    return p;
}

static inline thread_t* _reaper_get_thread()
{
    thread_t* thread;

    mutex_lock(__reaper_thread_lock);

    thread = queue_dequeue(__reaper_thread_queue);

    mutex_unlock(__reaper_thread_lock);

    /* Parent process is already in our queue, just fking wait a bit */
    if (thread && thread->m_parent_proc && (thread->m_parent_proc->m_flags & PROC_FINISHED) == PROC_FINISHED)
        return nullptr;

    return thread;
}

/*
 * TODO: should we have process destruction happen here, so execution isn't blocked,
 * or in reaper_on_packet, where we block untill the process is destroyed AND we're
 * most likely in an interrupt / critical section
 */
static void USED reaper_main()
{
    proc_t* proc;
    thread_t* thread;

    /* Check if we actually have a queue, otherwise try to create it again */
    if (!__reaper_process_queue)
        __reaper_process_queue = create_limitless_queue();

    if (!__reaper_process_lock)
        __reaper_process_lock = create_mutex(0);

    ASSERT_MSG(__reaper_process_queue, "Could not start reaper: unable to create queue");
    ASSERT_MSG(__reaper_process_lock, "Could not start reaper: unable to create mutex");

    /* Simply pass execution through */
    while (true) {

        thread = _reaper_get_thread();

        /* Found a thread that wants to die */
        if (thread) {
            /* Seperate it from its parent (We're evil) */
            if (thread->m_parent_proc)
                proc_remove_thread(thread->m_parent_proc, thread);

            /* Murder that bitch */
            destroy_thread(thread);
        }

        proc = _reaper_get_proc();

        if (proc)
            destroy_proc(proc);

        // scheduler_yield();
        if (!thread && !proc) {
            KLOG_DBG("Reaper: Nothing to be done, blockin...\n");
            thread_block(__reaper_thread);
        }
    }

    kernel_panic("Reaper thread isn't supposed to exit its loop!");
}

static void USED reaper_exit()
{
    kernel_panic("Reaper thread isn't supposed to exit!");
}

/*!
 * @brief: Asyncronically destroy a process and remove it from the scheduler
 *
 *
 */
kerror_t reaper_register_process(proc_t* proc)
{
    if (!proc)
        return -1;

    if (!__reaper_thread)
        return -1;

    /* TODO: If the reaper thread is idle, wake it up */
    ASSERT_MSG(!(__reaper_thread->m_parent_proc->m_flags & PROC_IDLE), "Kernelprocess seems to be idle!");

    /* Get the reaper lock so we know we can safely queue up the process */
    mutex_lock(__reaper_process_lock);

    /* Queue the process to the reaper */
    queue_enqueue(__reaper_process_queue, proc);

    /* Unlock the mutex. After this we musn't access @proc anymore */
    mutex_unlock(__reaper_process_lock);

    thread_unblock(__reaper_thread);
    return (0);
}

/*!
 * @brief: Hand over a thread to the reaper to kill it
 */
int reaper_register_thread(thread_t* thread)
{
    thread_set_state(thread, DEAD);

    /* Lock the reaper */
    mutex_lock(__reaper_thread_lock);

    /* Queue up the thread */
    queue_enqueue(__reaper_thread_queue, thread);

    /* Unlock the reaper */
    mutex_unlock(__reaper_thread_lock);

    thread_unblock(__reaper_thread);
    return 0;
}

kerror_t init_reaper(proc_t* proc)
{
    if (!proc || !(proc->m_flags & PROC_KERNEL))
        return -1;

    /* Make sure we know this is the process that contains the reaper */
    proc->m_flags |= PROC_REAPER;

    __reaper_port = 0;

    /*
     * Create the thread that our reaper will live on
     * repeat TODO: find usage for this thread being a socket, otherwise remove
     * perhaps we can even just move packets off of the interrupts,
     * so we can just lock mutexes and crap
     */
    __reaper_thread = create_thread_for_proc(proc, reaper_main, NULL, "Reaper");

    /* Create the mutex that ensures safety inside the reaper */
    __reaper_process_lock = create_mutex(NULL);
    __reaper_thread_lock = create_mutex(NULL);

    /* Queue with wich processes will be passed through */
    __reaper_process_queue = create_limitless_queue();
    __reaper_thread_queue = create_limitless_queue();

    proc_add_thread(proc, __reaper_thread);

    return (0);
}

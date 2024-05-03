#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "mem/kmem_manager.h"
#include "mem/page_dir.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "sync/atomic_ptr.h"
#include "system/resource.h"

struct penv;
struct thread;
struct oss_obj;
struct kresource;
struct proc_image;

/*
 * proc.h
 *
 * We define our blueprint for any process in the kernel here. Processes know about
 * a bunch of things like how many threads they have and which, what page directory
 * it uses, what resources its claiming, ect.
 * A bunch of things should be layed out a bit further:
 *
 * 1) resources
 * Every process owns a resource map. This is essensially an array that is indexed 
 * based on resource type and contains linked lists to all the resources, which are sorted
 * on address hight (a memory resource with the address 0x1fff gets placed before a memory
 * resource with the address 0xffff). Since resources also need to know if they are shared,
 * every resource also keeps a linked list of owners
 */

typedef struct proc_image {
  /* NOTE: make sure the low and high addresses are page-aligned */
  vaddr_t m_lowest_addr;
  vaddr_t m_highest_addr;
  size_t m_total_exe_bytes;
} proc_image_t;

inline bool proc_image_is_correctly_aligned(proc_image_t* image)
{
  return (ALIGN_UP(image->m_highest_addr, SMALL_PAGE_SIZE) == image->m_highest_addr &&
          ALIGN_DOWN(image->m_lowest_addr, SMALL_PAGE_SIZE) == image->m_lowest_addr);
}

inline void proc_image_align(proc_image_t* image) 
{
  image->m_highest_addr = ALIGN_UP(image->m_highest_addr, SMALL_PAGE_SIZE);
  image->m_lowest_addr = ALIGN_DOWN(image->m_lowest_addr, SMALL_PAGE_SIZE);
}

/*
 * This struct may get quite big, so let's make sure to put 
 * frequently used fields close to eachother in order to minimize 
 * cache-misses
 *
 * TODO: proc should prob be protected by atleast a few mutexes or spinlocks
 */
typedef struct proc {
  const char* m_name;
  struct oss_obj* obj;
  /* ASCII String which contains the runtime context of the process (aka the runtime parameters) */
  const char* m_runtime_ctx;
  proc_id_t m_id;
  uint32_t m_flags;

  /* This is used to compare a processes status in relation to other processes */
  struct penv* m_env;
  struct proc* m_parent;

  /* Resource tracking */
  page_dir_t m_root_pd;
  khandle_map_t m_handle_map;
  kresource_bundle_t* m_resource_bundle;

  struct thread* m_init_thread;
  struct thread* m_idle_thread;
  struct thread* m_prev_thread;
  list_t* m_threads;
  atomic_ptr_t* m_thread_count;

  kdoorbell_t* m_terminate_bell;

  size_t m_ticks_elapsed;

  /* Represent the image that this proc stems from (either from disk or in-ram) */
  struct proc_image m_image;
} proc_t;

/* We will probably find more usages for this */
#define PROC_IDLE           (0x00000001)
#define PROC_KERNEL         (0x00000002) /* This process runs directly in the kernel */
#define PROC_DRIVER         (0x00000004) /* This process runs with kernel privileges in its own pagemap */
#define PROC_STALLED        (0x00000008) /* This process ran as either a socket or a shared library and is now waiting detachment or an exit signal of some sort */
#define PROC_UNRUNNED       (0x00000010) /* V-card still intact */
#define PROC_FINISHED       (0x00000020) /* Process should be cleaned up by the scheduler */
#define PROC_REAPER         (0x00000040) /* Process capable of killing other processes and threads */
#define PROC_HAD_HANDLE     (0x00000080) /* Process is referenced in userspace by a handle */
#define PROC_SHOULD_STALL   (0x00000100) /* Process was launched as an entity that needs explicit signaling for actual exit and destruction */
#define PROC_NO_SOCKET      (0x00000200)
#define PROC_DID_REQUEST_RT (0x00000400)

proc_t* create_proc(proc_t* parent, struct user_profile* profile, proc_id_t* id_buffer, char* name, FuncPtr entry, uintptr_t args, uint32_t flags);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);

kerror_t proc_install_runtime(proc_t* proc, const char* rt);
kerror_t proc_get_runtime(proc_t* proc, const char** rt);

/* Block until the process has ended execution */
int await_proc_termination(proc_id_t id);
int proc_clone(proc_t* p, const char* clone_name, proc_t** clone);
kerror_t proc_set_entry(proc_t* p, FuncPtr entry, uintptr_t arg0, uintptr_t arg1);
const char* proc_try_get_symname(proc_t* proc, uintptr_t addr);

/*
 * Murder a proc object with all its threads as well.
 * TODO: we need to verify that these cleanups happen
 * gracefully, meaning that there is no leftover 
 * debris from killing all those threads. 
 * We really want threads to have their own heaps, so that 
 * we don't have to keep track of what they allocated...
 */
void destroy_proc(proc_t*);

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread);
kerror_t proc_remove_thread(proc_t* proc, struct thread* thread);
void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args);

ErrorOrPtr try_terminate_process(proc_t* proc);
ErrorOrPtr try_terminate_process_ex(proc_t* proc, bool defer_yield);

/* Heh? */
void terminate_process(proc_t* proc);
void proc_exit();

void proc_set_env(proc_t* proc, struct penv* env);

static inline bool proc_can_schedule(proc_t* proc) 
{
  if (!proc || (proc->m_flags & PROC_FINISHED) == PROC_FINISHED || (proc->m_flags & PROC_IDLE) == PROC_IDLE)
    return false;

  if (!proc->m_threads || !proc->m_thread_count || !proc->m_init_thread || !proc->m_init_thread->f_entry)
    return false;

  /* If none of the conditions above are met, it seems like we can schedule */
  return true;
}

/*
 * This means that the process will be removed from the scheduler queue
 * and will only be emplaced back once an external source has requested
 * that is will be
 */
void stall_process(proc_t* proc);

/*!
 * @brief: Checks if @proc is the process that runs the kernel
 */
static inline bool is_kernel(proc_t* proc) {
  return (proc->m_id == 0);
}

/*!
 * @brief: Checks if @proc is a process managed by the kernel
 */
static inline bool is_kernel_proc(proc_t* proc)
{
  return ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL);
}

static inline bool is_driver_proc(proc_t* proc)
{
  return ((proc->m_flags & PROC_DRIVER) == PROC_DRIVER);
}

#endif // !__ANIVA_PROC__

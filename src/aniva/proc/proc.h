#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "mem/base_allocator.h"
#include "mem/kmem_manager.h"
#include "mem/page_dir.h"
#include "mem/pg.h"
#include "proc/handle.h"
#include "sync/atomic_ptr.h"

struct thread;
struct kresource_mirror;
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
 */
typedef struct proc {
  char m_name[32];
  proc_id_t m_id;

  uint32_t m_flags;

  page_dir_t m_root_pd;

  khandle_map_t m_handle_map;

  /* Represent the image that this proc stems from (either from disk or in-ram) */
  struct proc_image m_image;

  // maps?
  list_t* m_threads;

  struct kresource_mirror* m_resources;

  struct thread* m_init_thread;
  struct thread* m_idle_thread;
  struct thread* m_prev_thread;

  size_t m_ticks_used;
  size_t m_requested_max_threads;

  atomic_ptr_t* m_thread_count;

} proc_t;

/* We will probably find more usages for this */
#define PROC_IDLE           (0x00000001)
#define PROC_KERNEL         (0x00000002) /* This process runs directly in the kernel */
#define PROC_DRIVER         (0x00000004) /* This process runs with kernel privileges in its own pagemap */
#define PROC_STALLED        (0x00000008) /* This process ran as either a socket or a shared library and is now waiting detachment or an exit signal of some sort */
#define PROC_UNRUNNED       (0x00000010) /* V-card still intact */
#define PROC_FINISHED       (0x00000020) /* Process should be cleaned up by the scheduler (TODO: let cleaning be done by a reaper thread/proc)*/
#define PROC_DEFERED_HEAP   (0x00000040) /* Wait with creating a heap */
#define PROC_REAPER         (0x00000080) /* Process capable of killing other processes and threads */
#define PROC_HAD_HANDLE     (0x00000100) /* Process is referenced in userspace by a handle */
#define PROC_SHOULD_STALL   (0x00000200) /* Process was launched as an entity that needs explicit signaling for actual exit and destruction */

proc_t* create_proc(char* name, FuncPtr entry, uintptr_t args, uint32_t flags);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);
proc_t* create_proc_from_path(const char* path);

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
void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args);

ErrorOrPtr try_terminate_process(proc_t* proc);

/* Heh? */
void terminate_process(proc_t* proc);

/*
 * Exit the current process
 */
void proc_exit();

bool proc_can_schedule(proc_t* proc);

/*
 * This means that the process will be removed from the scheduler queue
 * and will only be emplaced back once an external source has requested
 * that is will be
 */
void stall_process(proc_t* proc);

static ALWAYS_INLINE bool is_kernel_proc(proc_t* proc) {
  return (proc->m_id == 0);
}

#endif // !__ANIVA_PROC__

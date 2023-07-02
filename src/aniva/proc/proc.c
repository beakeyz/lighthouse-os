#include "proc.h"
#include <mem/heap.h>
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "entry/entry.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "interrupts/interrupts.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/handle.h"
#include "proc/kprocs/idle.h"
#include "proc/kprocs/reaper.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "system/processor/processor.h"
#include "system/resource.h"
#include "thread.h"
#include "libk/io.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>

proc_t* create_proc(char* name, FuncPtr entry, uintptr_t args, uint32_t flags) {
  size_t name_length;
  proc_t *proc;

  if (!name || !entry)
    return nullptr;

  /* TODO: create proc cache */
  proc = kmalloc(sizeof(proc_t));
  memset(proc, 0, sizeof(proc_t));

  if (proc == nullptr) {
    return nullptr;
  }

  proc->m_handle_map = create_khandle_map_ex(KHNDL_DEFAULT_ENTRIES);
  proc->m_id = Must(generate_new_proc_id());
  proc->m_flags = flags | PROC_UNRUNNED;
  proc->m_thread_count = create_atomic_ptr_with_value(0);

  // Only create new page dirs for non-kernel procs
  if ((flags & PROC_KERNEL) != PROC_KERNEL || (flags & PROC_DRIVER) == PROC_DRIVER) {
    proc->m_requested_max_threads = 3;
    proc->m_root_pd = kmem_create_page_dir(KMEM_CUSTOMFLAG_CREATE_USER, 0);
  } else {
    /* FIXME: should kernel processes just get the kernel page dir? prolly not lol */
    proc->m_root_pd.m_root = kmem_get_krnl_dir();
    proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
    proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
    proc->m_requested_max_threads = PROC_DEFAULT_MAX_THREADS;
  }

  /* TODO: move away from the idea of idle threads */
  proc->m_idle_thread = nullptr;

  proc->m_threads = init_list();

  name_length = strlen(name);

  if (name_length > 31)
    name_length = 31;

  memcpy(proc->m_name, name, name_length);
  proc->m_name[31] = NULL;

  /* NOTE: ->m_init_thread gets set by proc_add_thread */
  thread_t* thread = create_thread_for_proc(proc, entry, args, "main");
  Must(proc_add_thread(proc, thread));

  proc_register(proc);

  return proc;
}

#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc (FuncPtr entry, uintptr_t  args) {

  /* TODO: check */
  if (!IS_KERNEL_FUNC(entry)) {
    return nullptr;
  }

  /* TODO: don't limit to one name */
  return create_proc(PROC_CORE_PROCESS_NAME, entry, args, PROC_KERNEL);
}

proc_t* create_proc_from_path(const char* path) {
  kernel_panic("TODO: create_proc_from_path");
  return nullptr;
}

static void __proc_clear_shared_resources(proc_t* proc)
{
  /* 1) Loop over all the allocated resources by this process */
  /* 2) detach from every resource and then the resource manager should
   *    automatically destroy resources and merge their address ranges
   *    into neighboring resources
   */

  kresource_mirror_t* current_mirror;

  if (!proc->m_resources)
    return;

  current_mirror = proc->m_resources;
  
  while (current_mirror) {

    uintptr_t start = current_mirror->m_start;
    size_t size = current_mirror->m_size;

    /* Skip mirrors with size zero */
    if (!size)
      goto cycle;

    /* We only handle memory resources for now */
    /* TODO: destry other resource types */
    switch (current_mirror->m_type) {
      case KRES_TYPE_MEM:
        /*
         * TODO: propegate error 
         * NOTE: resource_release pops the according mirror from the 
         *       processes linked list
         */
        println("Trying to dealloc");
        Must(__kmem_dealloc(proc->m_root_pd.m_root, proc, start, size));
        println("deallocced");
        break;
      default:
        /* Skip this entry for now */
        goto cycle;
    }

    continue;
cycle:
    current_mirror = current_mirror->m_next;
  }
}

/*
 * Loop over all the open handles of this process and clear out the 
 * lingering references and objects
 */
static void __proc_clear_handles(proc_t* proc)
{
  khandle_map_t* map;
  khandle_t* current_handle;

  if (!proc)
    return;

  map = &proc->m_handle_map;

  for (uint32_t i = 0; i < map->max_count; i++) {
    current_handle = &map->handles[i];

    if (!current_handle->reference.kobj)
      continue;

    switch (current_handle->type) {
      case KHNDL_TYPE_FILE:
        {
          vnode_t* node = current_handle->reference.vobj->m_parent;
        
          ASSERT_MSG(node, "File handle vobj didn't have a parent node!");

          /* destroy_vobj also detaches from the vnode */
          //destroy_vobj(current_handle->reference.vobj);
          vn_close(node, current_handle->reference.vobj);

          break;
        }
      case KHNDL_TYPE_PROC:
      case KHNDL_TYPE_NONE:
      default:
        break;
    }

    destroy_khandle(current_handle);
  }
}

/*
 * Caller should ensure proc != zero
 */
void destroy_proc(proc_t* proc) {

  FOREACH(i, proc->m_threads) {
    /* Kill every thread */
    destroy_thread(i->data);
  }

  /* Yeet handles */
  __proc_clear_handles(proc);

  /* loop over all the resources of this process and release them by using __kmem_dealloc */
  __proc_clear_shared_resources(proc);

  /* Free everything else */
  destroy_atomic_ptr(proc->m_thread_count);
  destroy_list(proc->m_threads);

  destroy_khandle_map(&proc->m_handle_map);

  /* 
   * Kill the root pd if it has one, other than the currently active page dir. 
   * We already kinda expect this to only be called from kernel context, but 
   * you never know... For that we simply allow every page directory to be 
   * killed as long as we are not currently using it :clown: 
   */
  if (proc->m_root_pd.m_root != get_current_processor()->m_page_dir) {
    kmem_destroy_page_dir(proc->m_root_pd.m_root);
  }

  proc_unregister(proc->m_name);

  kfree(proc);
}


bool proc_can_schedule(proc_t* proc) {
  if (!proc || (proc->m_flags & PROC_FINISHED) || (proc->m_flags & PROC_IDLE))
    return false;

  if (!proc->m_threads || !proc->m_thread_count || !proc->m_init_thread)
    return false;

  /* If none of the conditions above are met, it seems like we can schedule */
  return true;
}

/*
 * Terminate the process, which means that we
 * 1) mark it as finished
 * 2) remove it from the scheduler
 * 3) Queue it to the reaper thread so It can be safely be disposed of
 */
ErrorOrPtr try_terminate_process(proc_t* proc) {

  /* Pause scheduler: can't yield and mutex is held */
  pause_scheduler();

  /* Mark as finished */
  proc->m_flags |= PROC_FINISHED;

  /* Remove from the scheduler */
  sched_remove_proc(proc);

  /* Register to the reaper */
  TRY(reap_result, reaper_register_process(proc));

  /* Resume scheduling */
  resume_scheduler();

  return Success(0);
}


void proc_exit() 
{
  try_terminate_process(get_current_proc());
  scheduler_yield();
}

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread) {
  if (!thread || !proc)
    return Error();

  ErrorOrPtr does_contain = list_indexof(proc->m_threads, thread);

  if (does_contain.m_status == ANIVA_SUCCESS) {
    return Error();
  }

  /* If this is the first thread, we need to make sure it gets ran first */
  if (proc->m_threads->m_length == 0 && proc->m_init_thread == nullptr) {
    proc->m_init_thread = thread;
    /* Ensure the schedulers picks up on this fact */
    proc->m_flags |= PROC_UNRUNNED;
  }

  uintptr_t current_thread_count = atomic_ptr_load(proc->m_thread_count);
  atomic_ptr_write(thread->m_tid, current_thread_count);
  atomic_ptr_write(proc->m_thread_count, current_thread_count+1);

  list_append(proc->m_threads, thread);
  return Success(0);
}

void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args) {
  // TODO: generate new unique name
  //list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
  kernel_panic("TODO: implement async task threads");
}

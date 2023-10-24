#include "proc.h"
#include <mem/heap.h>
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/handle.h"
#include "proc/kprocs/idle.h"
#include "proc/kprocs/reaper.h"
#include "proc/profile/profile.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include "system/resource.h"
#include "thread.h"
#include "libk/io.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>

/*!
 * @brief Allocate memory for a process and prepare it for execution
 *
 * This creates:
 * - the processes page-map if needed
 * - a main thread
 * - some structures to do process housekeeping
 *
 * TODO: remove sockets from existing
 */
proc_t* create_proc(proc_t* parent, proc_id_t* id_buffer, char* name, FuncPtr entry, uintptr_t args, uint32_t flags)
{
  proc_t *proc;
  /* NOTE: ->m_init_thread gets set by proc_add_thread */
  thread_t* init_thread;

  if (!name || !entry)
    return nullptr;

  /* TODO: create proc cache */
  proc = kzalloc(sizeof(proc_t));

  if (!proc)
    return nullptr;

  memset(proc, 0, sizeof(proc_t));

  init_khandle_map(&proc->m_handle_map, KHNDL_DEFAULT_ENTRIES);
  create_resource_bundle(&proc->m_resource_bundle);

  /* TODO: move away from the idea of idle threads */
  proc->m_idle_thread = nullptr;
  proc->m_parent = parent;
  proc->m_name = strdup(name);
  proc->m_id = Must(generate_new_proc_id());
  proc->m_flags = flags | PROC_UNRUNNED;
  proc->m_thread_count = create_atomic_ptr_with_value(0);
  proc->m_terminate_bell = create_doorbell(8, KDOORBELL_FLAG_BUFFERLESS);
  proc->m_threads = init_list();

  // Only create new page dirs for non-kernel procs
  if (!is_kernel_proc(proc) || is_driver_proc(proc)) {
    proc->m_requested_max_threads = 3;
    proc->m_root_pd = kmem_create_page_dir(KMEM_CUSTOMFLAG_CREATE_USER, 0);
  } else {
    /* FIXME: should kernel processes just get the kernel page dir? prolly not lol */
    proc->m_root_pd.m_root = kmem_get_krnl_dir();
    proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
    proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
    proc->m_requested_max_threads = PROC_DEFAULT_MAX_THREADS;
  }
  
  init_thread = create_thread_for_proc(proc, entry, args, "main");

  ASSERT_MSG(init_thread, "Failed to create main thread for process!");

  Must(proc_add_thread(proc, init_thread));

  proc_register(proc);

  if (id_buffer)
    *id_buffer = proc->m_id;

  return proc;
}

/* This should probably be done by kmem_manager lmao */
#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc (FuncPtr entry, uintptr_t  args) {

  /* TODO: check */
  if (!IS_KERNEL_FUNC(entry)) {
    return nullptr;
  }

  /* TODO: don't limit to one name */
  return create_proc(nullptr, nullptr, PROC_CORE_PROCESS_NAME, entry, args, PROC_KERNEL);
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

  kresource_t* start_resource;
  kresource_t* current;
  kresource_t* next;

  if (!*proc->m_resource_bundle)
    return;
  
  /*
   * NOTE: We don't always link through the list here, since 
   * __kmem_dealloc calls resource_release which will place 
   * the address of the next resource in ->m_resources
   */

  for (kresource_type_t type = 0; type < KRES_MAX_TYPE; type++) {

    /* Find the first resource of this type */
    current = start_resource = proc->m_resource_bundle[type];

    while (current) {

      next = current->m_next;

      uintptr_t start = current->m_start;
      size_t size = current->m_size;

      /* Skip mirrors with size zero or no references */
      if (!size || !current->m_shared_count) {
        goto next_resource;
      }

      /* TODO: destry other resource types */
      switch (current->m_type) {
        case KRES_TYPE_MEM:

          /* Should we dealloc or simply unmap? */
          if ((current->m_flags & KRES_FLAG_MEM_KEEP_PHYS) == KRES_FLAG_MEM_KEEP_PHYS) {

            /* Try to unmap */
            if (kmem_unmap_range_ex(proc->m_root_pd.m_root, start, GET_PAGECOUNT(size), KMEM_CUSTOMFLAG_RECURSIVE_UNMAP)) {

              /* Pre-emptively remove the flags, just in case this fails */
              current->m_flags &= ~KRES_FLAG_MEM_KEEP_PHYS;

              /* Yay, now release the thing */
              resource_release(start, size, start_resource);
            }

            break;
          }

          __kmem_dealloc_ex(proc->m_root_pd.m_root, proc->m_resource_bundle, start, size, false, true, true);
          break;
        default:
          /* Skip this entry for now */
          break;
      }

next_resource:
      if (current->m_next) {
        ASSERT_MSG(current->m_type == current->m_next->m_type, "Found type mismatch while cleaning process resources");
      }

      current = next;
      continue;
    }
  }

  /* Destroy the entire bundle, which deallocates the structures */
  destroy_resource_bundle(proc->m_resource_bundle);
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

    if (!current_handle->reference.kobj || current_handle->index == KHNDL_INVALID_INDEX)
      continue;

    destroy_khandle(current_handle);
  }
}

/*
 * Caller should ensure proc != zero
 */
void destroy_proc(proc_t* proc) 
{
  proc_t* check;

  check = find_proc_by_id(proc->m_id);
  
  if (check)
    proc_unregister((char*)proc->m_name);

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
  destroy_doorbell(proc->m_terminate_bell);

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

  kfree((void*)proc->m_name);
  kzfree(proc, sizeof(proc_t));
}


bool proc_can_schedule(proc_t* proc) {
  if (!proc || (proc->m_flags & PROC_FINISHED) == PROC_FINISHED || (proc->m_flags & PROC_IDLE) == PROC_IDLE)
    return false;

  if (!proc->m_threads || !proc->m_thread_count || !proc->m_init_thread)
    return false;

  /* If none of the conditions above are met, it seems like we can schedule */
  return true;
}

/*!
 * @brief Wait for a process to be terminated
 *
 * 
 */
int await_proc_termination(proc_id_t id)
{
  proc_t* proc;
  kdoor_t terminate_door;

  /* Pause the scheduler so we don't get fucked while registering the door */
  pause_scheduler();

  proc = find_proc_by_id(id);

  /*
   * If we can't find the process here, that probably means its already
   * terminated even before we could make this call
   */
  if (!proc) {
    resume_scheduler();
    return 0;
  }

  init_kdoor(&terminate_door, NULL, NULL);

  Must(register_kdoor(proc->m_terminate_bell, &terminate_door));

  /* Resume the scheduler so we don't die */
  resume_scheduler();

  /*
   * FIXME: when we try to register a door after the doorbell has already been destroyed
   * we can create a 'deadlock' here, since we are waiting for our door to be rang while
   * there is no doorbell at all
   */
  while (!kdoor_is_rang(&terminate_door))
    scheduler_yield();

  destroy_kdoor(&terminate_door);

  return 0;
}

/*
 * Terminate the process, which means that we
 * 1) mark it as finished
 * 2) remove it from the scheduler
 * 3) Queue it to the reaper thread so It can be safely be disposed of
 * 4) Remove it from the global register store, so that the id gets freed
 */
ErrorOrPtr try_terminate_process(proc_t* proc) 
{
  /* Pause scheduler: can't yield and mutex is held */
  pause_scheduler();

  /* Mark as finished */
  proc->m_flags |= PROC_FINISHED;

  /* Ring the doorbell, so waiters know this process has terminated */
  doorbell_ring(proc->m_terminate_bell);

  /* Remove from the scheduler */
  sched_remove_proc(proc);

  /* Register from the global register store */
  proc_unregister((char*)proc->m_name);

  /* Resume scheduling */
  resume_scheduler();

  /* Finally, register to the reaper after the process is fully cleared, so its held memory can be disposed of */
  return reaper_register_process(proc);
}

/*!
 * @brief Set the profile of a certain process
 *
 * Nothing to add here...
 */
void proc_set_profile(proc_t* proc, proc_profile_t* profile)
{
  if (!profile && proc->m_profile && proc->m_profile->proc_count)
    proc->m_profile->proc_count--;

  proc->m_profile = profile;

  if (profile)
    profile->proc_count++;
}

/*!
 * @brief Exit the current process
 *
 * Nothing to add here...
 */
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

  /* TODO: thread locking */
  thread->m_tid = current_thread_count;

  atomic_ptr_write(proc->m_thread_count, current_thread_count+1);

  list_append(proc->m_threads, thread);
  return Success(0);
}

void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args) {
  // TODO: generate new unique name
  //list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
  kernel_panic("TODO: implement async task threads");
}

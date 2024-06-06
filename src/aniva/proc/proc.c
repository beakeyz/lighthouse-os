#include "proc.h"
#include <mem/heap.h>
#include "dev/core.h"
#include "entry/entry.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "kevent/types/thread.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "lightos/driver/loader.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "oss/obj.h"
#include "proc/env.h"
#include "proc/handle.h"
#include "proc/kprocs/reaper.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/resource.h"
#include "thread.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>

static void __destroy_proc(proc_t* p);

static void _proc_init_pagemap(proc_t* proc)
{
  // Only create new page dirs for non-kernel procs
  if (!is_kernel_proc(proc) && !is_driver_proc(proc)) {
    proc->m_root_pd = kmem_create_page_dir(KMEM_CUSTOMFLAG_CREATE_USER, 0);
    return;
  }

  proc->m_root_pd.m_root = nullptr;
  proc->m_root_pd.m_phys_root = (paddr_t)kmem_get_krnl_dir();
  proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
  proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
}

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
proc_t* create_proc(proc_t* parent, struct user_profile* profile, proc_id_t* id_buffer, char* name, FuncPtr entry, uintptr_t args, uint32_t flags)
{
  proc_t *proc;
  /* NOTE: ->m_init_thread gets set by proc_add_thread */
  thread_t* init_thread;

  if (!name)
    return nullptr;

  if (!profile)
    profile = get_active_profile();

  /* TODO: create proc cache */
  proc = kmalloc(sizeof(proc_t));

  if (!proc)
    return nullptr;

  memset(proc, 0, sizeof(proc_t));

  /* TODO: move away from the idea of idle threads */
  proc->m_idle_thread = nullptr;
  proc->m_parent = parent;
  proc->m_name = strdup(name);
  proc->m_flags = flags | PROC_UNRUNNED;
  proc->m_thread_count = 1;
  proc->m_threads = init_list();
  proc->m_lock = create_mutex(NULL);
  proc->obj = create_oss_obj(name);

  /* Register ourselves */
  oss_obj_register_child(proc->obj, proc, OSS_OBJ_TYPE_PROC, __destroy_proc);

  /* Create a pagemap */
  _proc_init_pagemap(proc);

  /* Create a handle map */
  init_khandle_map(&proc->m_handle_map, KHNDL_DEFAULT_ENTRIES);

  /* Okay to pass a reference, since resource bundles should NEVER own this memory */
  proc->m_resource_bundle = create_resource_bundle(&proc->m_root_pd);
  
  init_thread = create_thread_for_proc(proc, entry, args, "main");

  ASSERT_MSG(init_thread, "Failed to create main thread for process!");

  Must(proc_add_thread(proc, init_thread));

  proc_register(proc, profile);

  if (id_buffer)
    *id_buffer = proc->m_id;

  return proc;
}

/* This should probably be done by kmem_manager lmao */
#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc (FuncPtr entry, uintptr_t  args) 
{
  user_profile_t* admin;

  /* TODO: check */
  if (!IS_KERNEL_FUNC(entry))
    return nullptr;

  /* Kernel processes always run inside the admin profile */
  admin = get_admin_profile();

  /* TODO: don't limit to one name */
  return create_proc(nullptr, admin, nullptr, PROC_CORE_PROCESS_NAME, entry, args, PROC_KERNEL);
}

/*!
 * @brief: Try to set the entry of a process
 */
kerror_t proc_set_entry(proc_t* p, FuncPtr entry, uintptr_t arg0, uintptr_t arg1)
{
  if (!p || !p->m_init_thread)
    return -KERR_INVAL;

  /* Can't set the entry of a process that has already been scheduled */
  if (p->m_init_thread->m_ticks_elapsed)
    return -KERR_INVAL;

  mutex_lock(p->m_init_thread->m_lock);

  p->m_init_thread->f_entry = (ThreadEntry)entry;

  thread_set_entrypoint(p->m_init_thread, entry, arg0, arg1);

  mutex_unlock(p->m_init_thread->m_lock);
  return KERR_NONE;
}

/*!
 * @brief: Create a duplicate process
 *
 * Make sure that there are no threads in this process that hold any mutexes, since the clone
 * can't hold these at the same time
 */
int proc_clone(proc_t* p, const char* clone_name, proc_t** clone)
{
  /* Create new 'clone' process */
  /* Copy handle map */
  /* Copy resource list */
  /* Copy thread state */
  /* ... */
  return 0;
}

static void __proc_clear_shared_resources(proc_t* proc)
{
  /* 1) Loop over all the allocated resources by this process */
  /* 2) detach from every resource and then the resource manager should
   *    automatically destroy resources and merge their address ranges
   *    into neighboring resources
   */

  if (!proc->m_resource_bundle)
    return;

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

static inline void __proc_kill_threads(proc_t* proc)
{
  list_t* temp_tlist;

  if (!proc)
    return;

  /* Create a temporary trampoline list */
  temp_tlist = init_list();

  /* Put all threads on a seperate temporary list */
  FOREACH(i, proc->m_threads) {
    /* Kill every thread */
    list_append(temp_tlist, i->data);
  }

  /*  */
  FOREACH(i, temp_tlist) {
    /* Make sure we remove the thread from the processes queue */
    proc_remove_thread(proc, i->data);

    /* Murder the bitch */
    destroy_thread(i->data);
  }

  destroy_list(temp_tlist);
  destroy_list(proc->m_threads);
}

/*
 * Caller should ensure proc != zero
 */
void __destroy_proc(proc_t* proc) 
{
  /* Yeet threads */
  __proc_kill_threads(proc);

  /* Yeet handles */
  __proc_clear_handles(proc);

  /* loop over all the resources of this process and release them by using __kmem_dealloc */
  __proc_clear_shared_resources(proc);

  /* Free everything else */
  destroy_mutex(proc->m_lock);
  destroy_khandle_map(&proc->m_handle_map);
  kfree((void*)proc->m_name);

  /* 
   * Kill the root pd if it has one, other than the currently active page dir. 
   * We already kinda expect this to only be called from kernel context, but 
   * you never know... For that we simply allow every page directory to be 
   * killed as long as we are not currently using it :clown: 
   */
  if (proc->m_root_pd.m_root != get_current_processor()->m_page_dir)
    kmem_destroy_page_dir(proc->m_root_pd.m_root);

  kfree(proc);
}

void destroy_proc(proc_t* proc)
{
  /* Unregister from the global register store */
  ASSERT_MSG(proc_unregister(proc->m_id) == 0, "Failed to unregister proc");

  /* Calls __destroy_proc */
  destroy_oss_obj(proc->obj);
}

static bool _await_proc_term_hook_condition(kevent_ctx_t* ctx, void* param)
{
  proc_t* param_proc = param;
  kevent_proc_ctx_t* proc_ctx;

  if (ctx->buffer_size != sizeof(*proc_ctx))
    return false;

  proc_ctx = ctx->buffer;

  /* Check if this is our process */
  if (proc_ctx->type != PROC_EVENTTYPE_DESTROY || proc_ctx->process->m_id != param_proc->m_id)
    return false;

  /* Yes! Fire the hook */
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
  char hook_name[32] = { 0 };

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

  sfmt(hook_name, "await_proc_termination_%d", id);

  kevent_add_poll_hook("proc", hook_name, _await_proc_term_hook_condition, proc);

  /* Resume the scheduler so we don't die */
  resume_scheduler();

  /* Wait for the process to be bopped */
  return kevent_await_hook_fire("proc", hook_name, NULL, NULL);
}

/*
 * Terminate the process, which means that we
 * 1) Remove it from the global register store, so that the id gets freed
 * 2) Queue it to the reaper thread so It can be safely be disposed of
 *
 * NOTE: don't remove from the scheduler here, but in the reaper
 */
ErrorOrPtr try_terminate_process(proc_t* proc)
{
  KLOG_DBG("try_terminate_process\n");
  return try_terminate_process_ex(proc, false);
}

/*!
 * @brief: Queues a process for termination
 *
 * Order of opperation (+ lockings)
 * 1) Freeze the scheduler so we don't get fucked out here (this function might
 *    get called from inside a process, which means this function needs to always
 *    return)
 * 2) Make sure the targeted process does not get scheduled again
 * 3) Register the process to the reaper process, so it can get destroyed
 * 4) Unpause the scheduler
 */
ErrorOrPtr try_terminate_process_ex(proc_t* proc, bool defer_yield)
{
  thread_t* c_thread;
  ErrorOrPtr result;

  if (!proc)
    return Error();

  mutex_lock(proc->m_lock);

  /* Check every thread to see if there are any pending syscalls */
  FOREACH(i, proc->m_threads) {
    c_thread = i->data;

    KLOG_DBG("Thread %s %s in syscall: %d\n", c_thread->m_name, SYSID_IS_VALID(c_thread->m_c_sysid) ?  "is" : "was last", c_thread->m_c_sysid);
    
    /* Wait until the thread finishes it's syscall */
    if (c_thread->m_c_sysid != SYSID_EXIT) {
      while (SYSID_IS_VALID(c_thread->m_c_sysid)) {
        KLOG_DBG("Waiting for syscall... %d\n", c_thread->m_c_sysid);

        /* Make the thread yield when it exits this syscall */
        SYSID_SET_VALID(c_thread->m_c_sysid, false);

        scheduler_yield();
      }
    }

    /* Not in a syscall, yay */
    thread_disable_scheduling(c_thread);
  }

  mutex_unlock(proc->m_lock);

  /* Pause the scheduler to make sure we're not fucked while doing this */
  pause_scheduler();

  /* Mark as finished, since we know we won't be seeing it again after we return from this call */
  proc->m_flags |= PROC_FINISHED;

  /* Remove from the scheduler (Pauses it) */
  (void)sched_remove_proc(proc);

  /* 
   * Register to the reaper 
   * NOTE: this also pauses the scheduler
   */
  result = reaper_register_process(proc);
  
  resume_scheduler();

  /* Yield to catch any terminates from within a process */
  if (!defer_yield)
    scheduler_yield();
  return result;
}

/*!
 * @brief Set the environment of a certain process
 *
 * Does not check for any privileges
 */
void proc_set_env(proc_t* proc, penv_t* env)
{
  if (proc->m_env)
    penv_remove_proc(proc->m_env, proc);

  penv_add_proc(env, proc);
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

/*!
 * @brief: Add a thread to @proc
 *
 * Locks the process and fails if we try to add a thread to a finished process
 */
ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread) 
{
  ErrorOrPtr result;
  kevent_thread_ctx_t thread_ctx = { 0 };

  if (!thread || !proc)
    return Error();

  result = Error();

  mutex_lock(proc->m_lock);

  if ((proc->m_flags & PROC_FINISHED) == PROC_FINISHED)
    goto unlock_and_exit;

  result = list_indexof(proc->m_threads, thread);

  if (result.m_status == ANIVA_SUCCESS)
    goto unlock_and_exit;

  result = Success(0);

  thread_ctx.thread = thread;
  thread_ctx.type = THREAD_EVENTTYPE_CREATE;
  /* TODO: smp */
  thread_ctx.new_cpu_id = 0;
  thread_ctx.old_cpu_id = 0;

  /* Fire the create event */
  kevent_fire("thread", &thread_ctx, sizeof(thread_ctx));

  pause_scheduler();

  /* If this is the first thread, we need to make sure it gets ran first */
  if (proc->m_threads->m_length == 0 && proc->m_init_thread == nullptr) {
    proc->m_init_thread = thread;
    /* Ensure the schedulers picks up on this fact */
    proc->m_flags |= PROC_UNRUNNED;
  }
  
  /* Add the thread to the processes list (NOTE: ->m_thread_count has already been updated at this point) */
  list_append(proc->m_threads, thread);

  /*
   * Only prepare the context here if we're not trying to add the init thread 
   * 
   * When adding a seperate thread to a process, we have time to alter the thread between creating it and
   * adding it. We don't have this time with the initial thread, which has it's context prepared right before it
   * is scheduled for the first time
   */
  if (proc->m_init_thread != thread)
    thread_prepare_context(thread);
  
  resume_scheduler();

unlock_and_exit:
  mutex_unlock(proc->m_lock);
  return result;
}

/*!
 * @brief: Remove a thread from the process
 */
kerror_t proc_remove_thread(proc_t* proc, struct thread* thread)
{
  kerror_t error;
  kevent_thread_ctx_t thread_ctx = { 0 };

  error = -KERR_INVAL;

  mutex_lock(proc->m_lock);

  if (!list_remove_ex(proc->m_threads, thread))
    goto unlock_and_exit;

  proc->m_thread_count--;

  thread_ctx.thread = thread;
  thread_ctx.type = THREAD_EVENTTYPE_DESTROY;
  /* TODO: smp */
  thread_ctx.new_cpu_id = 0;
  thread_ctx.old_cpu_id = 0;

  /* Fire the create event */
  (void)kevent_fire("thread", &thread_ctx, sizeof(thread_ctx));

unlock_and_exit:
  mutex_unlock(proc->m_lock);
  return error;
}

void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args) {
  // TODO: generate new unique name
  //list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
  kernel_panic("TODO: implement async task threads");
}

const char* proc_try_get_symname(proc_t* proc, uintptr_t addr)
{
  const char* buffer;

  if (!proc || !addr)
    return nullptr;

  dynldr_getfuncname_msg_t msg_block = {
    .pid = proc->m_id,
    .func_addr = (void*)addr,
  };

  if (IsError(driver_send_msg_a(
          DYN_LDR_URL,
          DYN_LDR_GET_FUNC_NAME,
          &msg_block,
          sizeof(msg_block),
          &buffer,
          sizeof(char*))) || !buffer || !strlen(buffer))
    return nullptr;

  return buffer;
}

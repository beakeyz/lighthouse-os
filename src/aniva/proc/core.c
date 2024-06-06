#include "core.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include <libk/data/hashmap.h>
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "oss/core.h"
#include "oss/obj.h"
#include "proc/env.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/profile/runtime.h"
#include "system/sysvar/var.h"

// TODO: fix this mechanism, it sucks
static zone_allocator_t* __thread_allocator;

thread_t* spawn_thread(char name[32], FuncPtr entry, uint64_t arg0) 
{
  proc_t* current;
  thread_t* thread;

  /* Don't be dumb lol */
  if (!entry)
    return nullptr;

  current = get_current_proc();

  /* No currently scheduling proc means we are fucked lol */
  if (!current)
    return nullptr;

  thread = create_thread_for_proc(current, entry, arg0, name);

  if (!thread)
    return nullptr;

  if (IsError(proc_add_thread(current, thread))) {
    /* Sadge */
    destroy_thread(thread);
    return nullptr;
  }

  return thread;
}

/*!
 * @brief: Find a process through oss
 *
 * Processes get attached to oss at the Runtime/ rootnode, under the node of the 
 * userprofile they where created under, and inside the process environment they 
 * have for themselves. Environment have the name of the process that created them
 * and are suffixed with the process_id of that process, in order to avoid duplicate
 * environment names
 */
proc_t* find_proc(const char* path) 
{
  oss_obj_t* obj;

  if (!path)
    return nullptr;

  obj = nullptr;

  if (KERR_ERR(oss_resolve_obj(path, &obj)))
    return nullptr;

  if (!obj || obj->type != OSS_OBJ_TYPE_PROC)
    return nullptr;

  /* Yay this object contains our thing =D */
  return oss_obj_unwrap(obj, proc_t);
}

uint32_t get_proc_count()
{
  return runtime_get_proccount();
}

/*!
 * @brief: Loop over all the current processes and call @f_callback
 * 
 * If @f_callback returns false, the loop is broken
 *
 * @returns: Wether we were able to walk the entire vector
 */
bool foreach_proc(bool (*f_callback)(struct proc*))
{
  kernel_panic("TODO: foreach_proc");
  return true;
}

thread_t* find_thread(proc_t* proc, thread_id_t tid) {
  
  thread_t* ret;

  if (!proc)
    return nullptr;

  FOREACH(i, proc->m_threads) {
    ret = i->data;

    if (ret->m_tid == tid)
      return ret;
  }

  return nullptr;
}

static int _assign_penv(proc_t* proc, user_profile_t* profile)
{
  int error;
  penv_t* env;
  size_t size = strlen(proc->m_name) + 14;
  char env_label_buf[size];

  /* If at this point we don't have a profile, default to user */
  if (!profile)
    profile = get_user_profile();

  /* If there is a parent, use that environment */
  if (proc->m_parent) {
    penv_add_proc(proc->m_parent->m_env, proc);
    return 0;
  }

  /* Yay */
  memset(env_label_buf, 0, size);

  /* Format the penv label */
  if (sfmt(env_label_buf, "%s_env", proc->m_name))
    return -1;

  /* Create a environment in the user profile */
  env = create_penv(env_label_buf, profile->priv_level, NULL);

  if (!env)
    return -1;

  /* Add to the profile */
  error = profile_add_penv(profile, env);

  /* Can't add this environment to the profile (duplicate?) */
  if (error) {
    /* Destroy this environment =/ */
    destroy_penv(env);
    return error;
  }

  return penv_add_proc(env, proc);
}

/*!
 * @brief: Register a process to the kernel
 */
ErrorOrPtr proc_register(struct proc* proc, user_profile_t* profile)
{
  int error;
  processor_t* cpu;
  kevent_proc_ctx_t ctx;

  mutex_lock(proc->m_lock);

  /* Try to assign a process environment */
  error = _assign_penv(proc, profile);

  mutex_unlock(proc->m_lock);

  /* Oops */
  if (error)
    return Error(); 

  cpu = get_current_processor();

  ctx.process = proc;
  ctx.type = PROC_EVENTTYPE_CREATE;
  ctx.new_cpuid = cpu->m_cpu_num;
  ctx.old_cpuid = cpu->m_cpu_num;

  /* Fire a funky kernel event */
  kevent_fire("proc", &ctx, sizeof(ctx));

  return Success(0);
}

/*!
 * @brief: Unregister a process from the kernel
 */
kerror_t proc_unregister(struct proc* proc)
{
  processor_t* cpu;
  kevent_proc_ctx_t ctx;

  mutex_lock(proc->m_lock);

  cpu = get_current_processor();

  ctx.process = proc;
  ctx.type = PROC_EVENTTYPE_DESTROY;
  ctx.new_cpuid = cpu->m_cpu_num;
  ctx.old_cpuid = cpu->m_cpu_num;

  kevent_fire("proc", &ctx, sizeof(ctx));

  /* Make sure the process is removed form its profile */
  penv_remove_proc(proc->m_env, proc);

  mutex_unlock(proc->m_lock);

  return 0;
}

/*!
 * @brief: Check if the current process is the kernel process
 *
 * NOTE: this checks wether the current process has the ID of the
 * kernel process!
 */
bool current_proc_is_kernel()
{
  proc_t* curr = get_current_proc();
  return is_kernel(curr);
}

/*!
 * @brief: Allocate a thread on the dedicated thread allocator
 */
thread_t* allocate_thread()
{
  return zalloc_fixed(__thread_allocator);
}

/*!
 * @brief: Deallocate a thread from the dedicated thread allocator
 */
void deallocate_thread(thread_t* thread)
{
  zfree_fixed(__thread_allocator, thread);
}

/*
 * Initialize:
 *  - socket registry
 *  - proc_id generation
 *  - tspacket caching
 *  - TODO: process caching with its own zone allocator
 */
ANIVA_STATUS init_proc_core() 
{
  __thread_allocator = create_zone_allocator(128 * Kib, sizeof(thread_t), NULL);

  init_sysvars();
  init_user_profiles();

  return ANIVA_SUCCESS;
}


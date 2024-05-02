#include "core.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include <libk/data/hashmap.h>
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/env.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/sysvar/var.h"

// TODO: fix this mechanism, it sucks
static atomic_ptr_t* __next_proc_id;

static spinlock_t* __core_socket_lock;
static vector_t* __proc_vect;
static mutex_t* __proc_mutex;

/*!
 * @brief Register a process to the process list
 *
 * Nothing to add here...
 */
static ErrorOrPtr __register_proc(proc_t* proc)
{
  ErrorOrPtr result;

  if (!proc || !__proc_vect)
    return Error();

  mutex_lock(__proc_mutex);

  result = vector_add(__proc_vect, &proc);

  mutex_unlock(__proc_mutex);

  return result;
}

/*!
 * @brief Unregister a process of a given name @name
 *
 * @returns: Success(...) with a pointer to the unregistered process on a successful
 * unregister, otherwise Error()
 */
static proc_t* __unregister_proc_by_id(proc_id_t id)
{
  proc_t* ret;
  
  if (!__proc_vect)
    return NULL;

  ret = NULL;

  mutex_lock(__proc_mutex);

  FOREACH_VEC(__proc_vect, i, j) {
    proc_t* p = *(proc_t**)i;

    if (p->m_id != id)
      continue;

    vector_remove(__proc_vect, j);
    ret = p;
    break;
  }

  mutex_unlock(__proc_mutex);
  return ret;
}

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

/*
 * TODO: have process ids be contained inside a bitmap.
 * This gives us more dynamic procid taking and releasing,
 * at the cost of a greater space complexity. For a max. capacity 
 * of 1024 processes we would need 16 qwords or 16 * 8 = 128 bytes
 * in our bitmap.
 */
ErrorOrPtr generate_new_proc_id() 
{
  uintptr_t next = atomic_ptr_read(__next_proc_id);

  /* Limit exceeded, since a proc_id is always 32-bit here (for now) */
  if (next > (uint64_t)0xFFFFFFFF) {
    return Error();
  }

  atomic_ptr_write(__next_proc_id, next + 1);
  return Success((uint32_t)next);
}

proc_t* find_proc_by_id(proc_id_t id)
{
  proc_t* ret;

  mutex_lock(__proc_mutex);

  ret = nullptr;

  FOREACH_VEC(__proc_vect, data, index) {

    if (!data)
      break;

    ret = *(proc_t**)data;

    if (!ret || ret->m_id == id)
      break;

    ret = nullptr;
  }

  mutex_unlock(__proc_mutex);

  return ret;
}

struct thread* find_thread_by_fid(full_proc_id_t fid) 
{
  u_fid_t __id;
  proc_t* p;

  __id.id = fid;

  p = find_proc_by_id(__id.proc_id);

  if (!p)
    return nullptr;

  return find_thread(p, __id.thread_id);
}

proc_t* find_proc(const char* name) {

  proc_t* ret;

  if (!name || !__proc_vect)
    return nullptr;

  mutex_lock(__proc_mutex);

  FOREACH_VEC(__proc_vect, data, index) {
    ret = *(proc_t**)data;

    if (strcmp(ret->m_name, name) == 0)
      break;

    ret = nullptr;
  }

  mutex_unlock(__proc_mutex);

  return ret;
}

uint32_t get_proc_count()
{
  if (!__proc_vect)
    return NULL;

  return __proc_vect->m_length;
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
  proc_t* current;

  mutex_lock(__proc_mutex);

  FOREACH_VEC(__proc_vect, data, index) {
    current = *(proc_t**)data;

    if (!f_callback(current)) {
      mutex_unlock(__proc_mutex);
      return false;
    }
  }

  mutex_unlock(__proc_mutex);
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

static int _assign_penv(proc_t* proc)
{
  penv_t* env;
  char env_label_buf[strlen(proc->m_name) + 14];

  /* If there is a parent, use that environment */
  if (proc->m_parent) {
    penv_add_proc(proc->m_parent->m_env, proc);
    return 0;
  }

  /* Format the penv label */
  if (sfmt(env_label_buf, "%s_%d", proc->m_name, proc->m_id))
    return -1;

  /* Create a environment in the user profile */
  env = create_penv(env_label_buf, PRIV_LVL_USER, NULL);

  if (!env)
    return -1;

  /* Add to the profile */
  profile_add_penv(get_user_profile(), env);

  return penv_add_proc(env, proc);
}

/*!
 * @brief: Register a process to the kernel
 */
ErrorOrPtr proc_register(proc_t* proc)
{
  ErrorOrPtr result;
  processor_t* cpu;
  kevent_proc_ctx_t ctx;

  result = __register_proc(proc);

  if (IsError(result))
    return result;

  /* Try to assign a process environment */
  if (KERR_ERR(_assign_penv(proc)))
    return Error(); 

  cpu = get_current_processor();

  ctx.process = proc;
  ctx.type = PROC_EVENTTYPE_CREATE;
  ctx.new_cpuid = cpu->m_cpu_num;
  ctx.old_cpuid = cpu->m_cpu_num;

  /* Fire a funky kernel event */
  kevent_fire("proc", &ctx, sizeof(ctx));

  return result;
}

/*!
 * @brief: Unregister a process from the kernel
 */
kerror_t proc_unregister(proc_id_t id)
{
  proc_t* p;
  processor_t* cpu;
  kevent_proc_ctx_t ctx;

  p = __unregister_proc_by_id(id);

  if (!p)
    return -1;

  /* Make sure the process is removed form its profile */
  penv_remove_proc(p->m_env, p);

  cpu = get_current_processor();

  ctx.process = p;
  ctx.type = PROC_EVENTTYPE_DESTROY;
  ctx.new_cpuid = cpu->m_cpu_num;
  ctx.old_cpuid = cpu->m_cpu_num;

  kevent_fire("proc", &ctx, sizeof(ctx));

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

/*
 * Initialize:
 *  - socket registry
 *  - proc_id generation
 *  - tspacket caching
 *  - TODO: process caching with its own zone allocator
 */
ANIVA_STATUS init_proc_core() {

  __core_socket_lock = create_spinlock();
  __next_proc_id = create_atomic_ptr_ex(0);

  /*
   * TODO: we can also just store processes in a vector, since we
   * have the PROC_SOFTMAX that limits process creation
   */
  __proc_vect = create_vector(PROC_SOFTMAX, sizeof(proc_t*), VEC_FLAG_NO_DUPLICATES);

  __proc_mutex = create_mutex(NULL);

  //Must(create_kevent("proc_terminate", KEVENT_TYPE_CUSTOM, NULL, 8));

  init_sysvars();
  init_user_profiles();


  return ANIVA_SUCCESS;
}


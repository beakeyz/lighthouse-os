#include "core.h"
#include "dev/debug/serial.h"
#include "kevent/kevent.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include <libk/data/hashmap.h>
#include "libk/stddef.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/tspckt.h"
#include "proc/kprocs/socket_arbiter.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "socket.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

static uint32_t s_highest_port_cache;
// TODO: fix this mechanism, it sucks
static atomic_ptr_t* __next_proc_id;

static spinlock_t* __core_socket_lock;
/* FIXME: I don't like the fact that this is a fucking linked list */
static list_t *__sockets;
static vector_t* __proc_vect;
static mutex_t* __proc_mutex;

static void revalidate_port_cache();

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
static ErrorOrPtr __unregister_proc_by_name(const char* name)
{
  ErrorOrPtr result = Error();
  
  if (!name || !__proc_vect)
    return Error();

  mutex_lock(__proc_mutex);

  FOREACH_VEC(__proc_vect, i, j) {
    proc_t* p = *(proc_t**)i;

    if (strcmp(name, p->m_name) == 0) {
      vector_remove(__proc_vect, j);
      result = Success((uintptr_t)p);
      break;
    }
  }

  mutex_unlock(__proc_mutex);

  return result;
}

ErrorOrPtr destroy_relocated_thread_entry_stub(struct thread* thread) {

  processor_t* current;
  page_dir_t* dir;
  size_t stub_size;
  size_t aligned_size;
  
  if (!thread)
    return Error();

  dir = &thread->m_parent_proc->m_root_pd;
  stub_size = ((uintptr_t)&thread_entry_stub_end - (uintptr_t)&thread_entry_stub);
  aligned_size = ALIGN_UP(stub_size, SMALL_PAGE_SIZE);

  current = get_current_processor();

  if (current->m_page_dir != kmem_get_krnl_dir()) {
    return Error();
  }

  return __kmem_dealloc_unmap(dir->m_root, thread->m_parent_proc->m_resource_bundle, (uintptr_t)thread->f_relocated_entry_stub, aligned_size);
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
ErrorOrPtr generate_new_proc_id() {
  uintptr_t next = atomic_ptr_load(__next_proc_id);

  /* Limit exceeded, since a proc_id is always 32-bit here (for now) */
  if (next > (uint64_t)0xFFFFFFFF) {
    return Error();
  }

  atomic_ptr_write(__next_proc_id, next + 1);
  return Success((uint32_t)next);
}

list_t get_registered_sockets() {
  if (!__sockets) {
    list_t ret = {0};
    return ret;
  }
  return *__sockets;
}

ErrorOrPtr socket_register(threaded_socket_t* socket) {
  if (socket == nullptr) {
    return Error();
  }

  if (socket_is_flag_set(socket, TS_REGISTERED)) {
    return Error();
  }

  uint32_t port = socket_verify_port(socket);

  socket->m_port = port;

  if (port > s_highest_port_cache) {
    s_highest_port_cache = port;
  }

  revalidate_port_cache();

  socket_set_flag(socket, TS_REGISTERED, true);

  /* Let the arbiter know we exist */
  Must(socket_arbiter_register_socket(socket));

  /*
   * TODO: sort based on port value (low -> high)
   */
  list_append(__sockets, socket);
  return Success(0);
}

ErrorOrPtr socket_unregister(threaded_socket_t* socket) {
  if (socket == nullptr) {
    return Error();
  }

  if (!socket_is_flag_set(socket, TS_REGISTERED)) {
    return Error();
  }

  socket_set_flag(socket, TS_REGISTERED, false);

  uint32_t socket_port = socket->m_port;

  ErrorOrPtr result = list_indexof(__sockets, socket);

  if (result.m_status == ANIVA_FAIL) {
    return Error();
  }

  list_remove(__sockets, Release(result));

  if (s_highest_port_cache > socket_port) {
    // reset and rebind to the highest port
    s_highest_port_cache = 0;
    FOREACH(i, __sockets) {
      threaded_socket_t* check = i->data;
      if (check->m_port > s_highest_port_cache) {
        s_highest_port_cache = check->m_port;
      }
    }
  }

  Must(socket_arbiter_remove_socket(socket));

  return Success(0);
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


ErrorOrPtr proc_register(proc_t* proc)
{
  ErrorOrPtr result;

  result = __register_proc(proc);

  if (IsError(result))
    return result;

  /* Register to global if we are from the kernel */
  if (is_kernel_proc(proc))
    proc_register_to_global(proc);
  /*
   * Little TODO
   *
   * When the process is not a kernel process, that means it was probably 
   * launched by a user, in which case we need to know what profile has 
   * invoked the process creation and give this process the same profile
   *
   * For now we register to global
   */
  else
    proc_register_to_global(proc);

  return result;
}

ErrorOrPtr proc_unregister(char* name) 
{
  proc_t* p;
  ErrorOrPtr result;

  result = __unregister_proc_by_name(name);

  if (IsError(result))
    return result;

  p = (proc_t*)Release(result);

  if (!p)
    return Error();

  /* Make sure the process is removed form its profile */
  proc_set_profile(p, nullptr);

  return result;
}

threaded_socket_t *find_registered_socket(uint32_t port) {
  FOREACH(i, __sockets) {
    threaded_socket_t *socket = i->data;
    if (socket && socket->m_port == port) {
      return socket;
    }
  }
  return nullptr;
}

ErrorOrPtr socket_try_verifiy_port(threaded_socket_t* socket) {

  bool is_duplicate = false;

  FOREACH(i, __sockets) {
    threaded_socket_t* check_socket = i->data;

    if (socket->m_port == check_socket->m_port) {
      is_duplicate = true;
      break;
    }
  }

  if (!is_duplicate)
    return Success(socket->m_port);

  uint32_t new_port = s_highest_port_cache + 1;

  FOREACH(i, __sockets) {
    threaded_socket_t* check_socket = i->data;

    if (check_socket->m_port >= new_port) {
      // our cache is invalid, return an error
      return Error();
    }
  }

  // no match, success!
  socket->m_port = new_port;
  s_highest_port_cache = new_port;
  return Success(new_port);
}

uint32_t socket_verify_port(threaded_socket_t* socket) {
  ErrorOrPtr result = socket_try_verifiy_port(socket);

  if (result.m_status == ANIVA_SUCCESS) {
    return (uint32_t)result.m_ptr;
  }

  revalidate_port_cache();
  result = socket_try_verifiy_port(socket);

  if (result.m_status != ANIVA_SUCCESS) {
    // if we STILL can't find anything, just bruteforce it...
    uint32_t new_port = s_highest_port_cache + 1;
    bool is_duplicate = false;

    while (true) {
      FOREACH(i, __sockets) {
        threaded_socket_t* check_socket = i->data;
        if (check_socket->m_port == new_port) {
          is_duplicate = true;
          break;
        }
      }
      if (!is_duplicate) {
        socket->m_port = new_port;
        return new_port;
      }
      new_port++;
    }
  }

  return (uint32_t)result.m_ptr;
}

static void revalidate_port_cache() {
  FOREACH(i, __sockets) {
    threaded_socket_t* check_socket = i->data;

    if (check_socket->m_port >= s_highest_port_cache) {
      s_highest_port_cache = check_socket->m_port;
    }
  }
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

  __sockets = init_list();
  __core_socket_lock = create_spinlock();
  __next_proc_id = create_atomic_ptr_with_value(0);

  /*
   * TODO: we can also just store processes in a vector, since we
   * have the PROC_SOFTMAX that limits process creation
   */
  __proc_vect = create_vector(PROC_SOFTMAX, sizeof(proc_t*), VEC_FLAG_NO_DUPLICATES);

  __proc_mutex = create_mutex(NULL);

  //Must(create_kevent("proc_terminate", KEVENT_TYPE_CUSTOM, NULL, 8));

  init_tspckt();
  init_socket();
  init_packet_payloads();
  init_packet_response();

  init_proc_variables();
  init_proc_profiles();


  return ANIVA_SUCCESS;
}


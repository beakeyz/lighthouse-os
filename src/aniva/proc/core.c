#include "core.h"
#include "dev/debug/serial.h"
#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include <libk/data/hashmap.h>
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/tspckt.h"
#include "proc/kprocs/socket_arbiter.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "socket.h"
#include "sync/atomic_ptr.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

static uint32_t s_highest_port_cache;
// TODO: fix this mechanism, it sucks
static atomic_ptr_t* __next_proc_id;

static spinlock_t* __core_socket_lock;
/* FIXME: I don't like the fact that this is a fucking linked list */
static list_t *__sockets;
static hashmap_t* __proc_map;

static void revalidate_port_cache();

static ErrorOrPtr __register_proc(proc_t* proc)
{
  if (!proc || !__proc_map)
    return Error();

  return hashmap_put(__proc_map, proc->m_name, proc);
}

static ErrorOrPtr __unregister_proc_by_name(const char* name)
{
  hashmap_key_t key;
  
  if (!name || !__proc_map)
    return Error();

  key = (hashmap_key_t)name;

  return hashmap_remove(__proc_map, key);
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

  println("Dealloc");
  if (current->m_page_dir != kmem_get_krnl_dir()) {
    return Error();
  }

  return __kmem_dealloc_unmap(dir->m_root, thread->m_parent_proc->m_resource_bundle, (uintptr_t)thread->f_relocated_entry_stub, aligned_size);
}

ErrorOrPtr spawn_thread(char name[32], FuncPtr entry, uint64_t arg0) {

  /* Don't be dumb lol */
  if (!entry)
    return Error();

  proc_t* current = get_current_proc();

  /* No currently scheduling proc means we are fucked lol */
  if (!current)
    return Error();

  thread_t* thread = create_thread_for_proc(current, entry, arg0, name);

  if (!thread)
    return Error();

  return proc_add_thread(current, thread);
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

static ErrorOrPtr __find_proc_by_id(void* p, uint64_t id)
{
  proc_t* proc = (proc_t*)p;
  proc_id_t pid = (proc_id_t)id;

  /* Yay, found it */
  if (proc->m_id == pid)
    return Error();

  return Success(0);
}

proc_t* find_proc_by_id(proc_id_t id)
{
  proc_t* ret;
  ErrorOrPtr result;

  result = hashmap_itterate(__proc_map, __find_proc_by_id, id);

  /* A successful itteration means we didn't find the proc we where looking for */
  if (!IsError(result))
    return nullptr;

  ret = (proc_t*)Release(result);

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
  hashmap_key_t key;

  if (!name || !__proc_map)
    return nullptr;

  key = (hashmap_key_t)name;

  ret = hashmap_get(__proc_map, key);

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


ErrorOrPtr proc_register(struct proc* proc)
{
  ErrorOrPtr result;

  result = __register_proc(proc);

  return result;
}

ErrorOrPtr proc_unregister(char* name) 
{
  ErrorOrPtr result;

  result = __unregister_proc_by_name(name);

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
  __proc_map = create_hashmap(PROC_SOFTMAX, HASHMAP_FLAG_CA);

  //Must(create_kevent("proc_terminate", KEVENT_TYPE_CUSTOM, NULL, 8));

  init_tspckt();
  init_socket();
  init_packet_payloads();
  init_packet_response();

  return ANIVA_SUCCESS;
}


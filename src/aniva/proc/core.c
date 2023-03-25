#include "core.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
#include "libk/vector.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "socket.h"
#include "sync/atomic_ptr.h"
#include "sync/spinlock.h"

static list_t *s_sockets;
static uint32_t s_highest_port_cache;

static spinlock_t* s_core_socket_lock;

// TODO: fix this mechanism, it sucks
static atomic_ptr_t* next_proc_id;

static void revalidate_port_cache();

/*
 * Initialize:
 *  - socket registry
 *  - proc_id generation
 */
ANIVA_STATUS initialize_proc_core() {

  s_sockets = init_list();
  s_core_socket_lock = create_spinlock();
  next_proc_id = create_atomic_ptr_with_value(1);

  return ANIVA_SUCCESS;
}

// Leaves the current thread/process behind to be scheduled back into later
ErrorOrPtr exec_user(char proc_name[32], FuncPtr entry, uintptr_t arg0, uintptr_t arg1) {

  pause_scheduler();
  proc_t* user_proc = create_clean_proc(proc_name, Must(generate_new_proc_id()));

  proc_add_thread(user_proc, create_thread_for_proc(user_proc, entry, arg0, "mainthread"));


  sched_add_proc(user_proc);

  resume_scheduler();

  scheduler_yield();

  return Success(0);
}

// FIXME: EWWWWWWW
ErrorOrPtr generate_new_proc_id() {
  uintptr_t next = atomic_ptr_load(next_proc_id);
  atomic_ptr_write(next_proc_id, next + 1);
  return Success(next);
}

list_t get_registered_sockets() {
  if (!s_sockets) {
    list_t ret = {0};
    return ret;
  }
  return *s_sockets;
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

  // TODO: more verification

  socket_set_flag(socket, TS_REGISTERED, true);
  list_append(s_sockets, socket);
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

  ErrorOrPtr result = list_indexof(s_sockets, socket);

  if (result.m_status == ANIVA_FAIL) {
    return Error();
  }

  list_remove(s_sockets, Release(result));

  if (s_highest_port_cache > socket_port) {
    // reset and rebind to the highest port
    s_highest_port_cache = 0;
    FOREACH(i, s_sockets) {
      threaded_socket_t* check = i->data;
      if (check->m_port > s_highest_port_cache) {
        s_highest_port_cache = check->m_port;
      }
    }
  }
  return Success(0);
}

threaded_socket_t *find_registered_socket(uint32_t port) {
  FOREACH(i, s_sockets) {
    threaded_socket_t *socket = i->data;
    if (socket && socket->m_port == port) {
      return socket;
    }
  }
  return nullptr;
}

ErrorOrPtr socket_try_verifiy_port(threaded_socket_t* socket) {

  bool is_duplicate = false;

  FOREACH(i, s_sockets) {
    threaded_socket_t* check_socket = i->data;

    if (socket->m_port == check_socket->m_port) {
      is_duplicate = true;
      break;
    }
  }

  if (!is_duplicate)
    return Success(socket->m_port);

  uint32_t new_port = s_highest_port_cache + 1;

  FOREACH(i, s_sockets) {
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
      FOREACH(i, s_sockets) {
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
  FOREACH(i, s_sockets) {
    threaded_socket_t* check_socket = i->data;

    if (check_socket->m_port >= s_highest_port_cache) {
      s_highest_port_cache = check_socket->m_port;
    }
  }
}

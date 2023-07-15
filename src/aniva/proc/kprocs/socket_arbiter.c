#include "socket_arbiter.h"
#include "dev/debug/serial.h"
#include "interrupts/interrupts.h"
#include "libk/data/queue.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"

mutex_t __arbiter_lock;
vector_t __port_vector;
thread_t* __arbiter_thread;

static bool __can_try_handle_here(threaded_socket_t* socket)
{
  /* When the socket is a userthread, we can't simply execute it here =/ */
  return (((socket->m_parent->m_parent_proc->m_flags & PROC_KERNEL) == PROC_KERNEL ||
      (socket->m_parent->m_parent_proc->m_flags & PROC_KERNEL) == PROC_DRIVER));
}

static void __prepare_user_packet(threaded_socket_t* socket) 
{
  /* Try to get into a context where we can call a userfunction in usercontext and 
   * uppon return of the function we return here in kernel-mode (like a syscall return)
   */
  disable_interrupts();

  kernel_panic("Try to handle userpacket");

  socket_set_flag(socket, TS_SHOULD_HANDLE_USERPACKET, true);

  /* Just to be sure */
  socket_set_flag(socket, TS_HANDLING_USERPACKET, false);

  enable_interrupts();
}

void socket_arbiter_entry()
{
  ErrorOrPtr result;
  uint32_t* current_port;
  threaded_socket_t* current_socket;

  while (true) {

    mutex_lock(&__arbiter_lock);

    FOREACH_VEC(&__port_vector, i) {

      current_port = (uint32_t*)i;
      
      current_socket = find_registered_socket(*current_port);

      /* Some kind of failure, remove this port */
      if (!current_socket)
        break;

      /* Skip sockets that are currently handling userpackets */
      if (!current_socket->f_on_packet || socket_is_flag_set(current_socket, TS_SHOULD_HANDLE_USERPACKET) || socket_is_flag_set(current_socket, TS_HANDLING_USERPACKET))
        goto cycle;

      /* When we can't directly handle packets here, we should look to  */
      if (!__can_try_handle_here(current_socket)) {

        if (!socket_has_userpacket_handler(current_socket)) {
          /*
           * Silently remove the handler, since it seems to be invalid 
           * FIXME: locking? 
           */
          current_socket->f_on_packet = NULL;
          goto cycle;
        }

        /* If there is no packet to handle currently, we just cycle through */
        if (!queue_peek(current_socket->m_packet_queue.m_packets))
          goto cycle;

        /* If there is, we need to prepare and yield */
        __prepare_user_packet(current_socket);
        goto unlock_and_yield;
      }

      /* Try to handle any packets that the socket might have */
      result = socket_try_handle_tspacket(current_socket);

      /* If we succeeded to handle this fucker, just yield */
      if (!IsError(result))
        goto unlock_and_yield;

cycle:
      current_socket = nullptr;
      current_port = nullptr;
    }

    /*
     * If we exit the vector loop while the current_port is non-null, that means there was a rogue
     * entry in our port buffer. We'll need to remove it
     */
    if (current_port) {
      uint32_t index = Must(vector_indexof(&__port_vector, current_port));

      Must(vector_remove(&__port_vector, index));
    }

unlock_and_yield:
    mutex_unlock(&__arbiter_lock);
    scheduler_yield();
  }

  kernel_panic("socket arbiter tried to exit!");
}

void init_socket_arbiter(proc_t* proc)
{
  if (!proc || !(proc->m_flags & PROC_KERNEL))
    return;

  ASSERT_MSG(is_kernel_proc(proc), "Can't initialize the socket arbiter in a non-kernel environment!");

  init_mutex(&__arbiter_lock, NULL);
  init_vector(&__port_vector, 128, sizeof(uint32_t), VEC_FLAG_FLEXIBLE | VEC_FLAG_NO_DUPLICATES);

  __arbiter_thread = create_thread_for_proc(proc, socket_arbiter_entry, NULL, "socket_arbiter");

  ASSERT_MSG(__arbiter_thread, "Failed to create socket arbiter thread!");

  Must(proc_add_thread(proc, __arbiter_thread));
}

ErrorOrPtr socket_arbiter_register_socket(threaded_socket_t* socket) 
{
  ErrorOrPtr result;

  if (!socket)
    return Error();

  mutex_lock(&__arbiter_lock);

  result = vector_add(&__port_vector, &socket->m_port);

  mutex_unlock(&__arbiter_lock);

  return result;
}

ErrorOrPtr socket_arbiter_remove_socket(threaded_socket_t* socket)
{
  ErrorOrPtr status;

  if (!socket)
    return Error();

  mutex_lock(&__arbiter_lock);

  status = vector_indexof(&__port_vector, &socket->m_port);

  if (IsError(status))
    goto fail_and_unlock;

  status = vector_remove(&__port_vector, Release(status));

  mutex_unlock(&__arbiter_lock);
  return status;

fail_and_unlock:
  mutex_unlock(&__arbiter_lock);
  return Error();
}

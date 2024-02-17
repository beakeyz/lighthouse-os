#ifndef __ANIVA_PROC_CORE__
#define __ANIVA_PROC_CORE__

#include "dev/core.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/data/vector.h"
#include "libk/data/queue.h"

#define DEFAULT_STACK_SIZE                      (32 * Kib)
#define DEFAULT_THREAD_MAX_TICKS                (10)

#define PROC_DEFAULT_MAX_THREADS                (16)
#define PROC_CORE_PROCESS_NAME                  "[aniva-core]"
#define PROC_MAX_TICKS                          (20)

#define PROC_SOFTMAX                            (0x1000)

/* The 32-bit limit for proc ids */
#define PROC_HARDMAX                            (0xFFFFFFFF)

#define THREAD_HARDMAX                          (0xFFFFFFFF)

#define MIN_SOCKET_BUFFER_SIZE                  (0)
// FIXME: should this be the max size?
#define SOCKET_DEFAULT_SOCKET_BUFFER_SIZE       (4096)
#define SOCKET_DEFAULT_MAXIMUM_SOCKET_COUNT     (128)
#define SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT     (64)

extern char thread_entry_stub[];
extern char thread_entry_stub_end[];

struct proc;
struct thread;
struct tspckt;
struct drv_manifest;
struct threaded_socket;
struct packet_response;

typedef enum thread_state {
  INVALID = 0,      // not initialized
  RUNNING,          // executing
  RUNNABLE,         // can be executed by the scheduler
  DYING,            // waiting to be cleaned up
  DEAD,             // thread is destroyed, the scheduler can remove it from the pool
  STOPPED,          // stopped by the scheduler, for whatever reason. waiting for reschedule
  BLOCKED,          // performing blocking operation
  SLEEPING,         // waiting for anything to happen (i.e. signals, data)
  NO_CONTEXT,       // waiting to recieve a context to run
} thread_state_t;

ANIVA_STATUS init_proc_core();

/*
 * Register a socket on the kernel socket chain
 */
ErrorOrPtr socket_register(struct threaded_socket* socket);

/*
 * unregister a socket from the kernel socket chain
 */
ErrorOrPtr socket_unregister(struct threaded_socket* socket);


typedef int thread_id_t;
typedef int proc_id_t;


/* 64 bit value that combines the tid and the pid together */
typedef uint64_t full_proc_id_t;
typedef full_proc_id_t fid_t;

typedef union {
  struct {
    thread_id_t thread_id;
    proc_id_t proc_id;
  };
  fid_t id;
} u_full_proc_id_t, u_fid_t;


static inline full_proc_id_t create_full_procid(uint32_t proc_id, uint32_t thread_id)
{
  u_full_proc_id_t ret;

  ret.proc_id = proc_id;
  ret.thread_id = thread_id;

  return ret.id;
}


static inline uint32_t full_procid_get_tid(full_proc_id_t fprocid)
{
  u_full_proc_id_t u_id;

  u_id.id = fprocid;

  return u_id.thread_id;
}


static inline proc_id_t full_procid_get_pid(full_proc_id_t fprocid)
{
  u_full_proc_id_t u_id;

  u_id.id = fprocid;

  return u_id.proc_id;
}


/*
 * Try to grab a new proc_id
 */
ErrorOrPtr generate_new_proc_id();

/*
 * return a pointer to the socket register
 */
list_t get_registered_sockets();

/*
 * Spawn a thread for the current running process. Does not
 * care about if we are in usermode or kernelland
 */
struct thread* spawn_thread(char name[32], FuncPtr entry, uint64_t arg0);

/*
 * find a socket based on its port
 * TODO: validate port based on checksum?
 */
struct threaded_socket *find_registered_socket(uint32_t port);

struct proc* find_proc_by_id(proc_id_t id);
struct proc* find_proc(const char* name);

bool foreach_proc(bool (*f_callback)(struct proc*));
uint32_t get_proc_count();

struct thread* find_thread_by_fid(full_proc_id_t fid);
struct thread* find_thread(struct proc* proc, thread_id_t tid);

ErrorOrPtr proc_register(struct proc* proc);
ErrorOrPtr proc_unregister(char* name);

bool current_proc_is_kernel();

/*
 * send a data-packet to a port
 * returns a pointer to the response ptr (thus a double pointer)
 * if all goes well, otherwise a nullptr
 */
extern ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * Send a packet to the socket and discard the response buffer
 */
extern ErrorOrPtr send_packet_to_socket_ex(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size); // socket.c

/*
 * validata a tspckt based on its identifier (checksum, hash, idk man)
 */
extern bool validate_tspckt(struct tspckt* packet); // tspctk.c

/*
 * Utilises CRC32 to generate a 32-bit checksum, based on the packet struct, its buffer, and its sender thread
 */
extern ErrorOrPtr generate_tspckt_identifier(struct tspckt* packet); // tspckt.c

#endif //__LIGHTHOUSE_OS_CORE__

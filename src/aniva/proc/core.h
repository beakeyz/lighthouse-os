#ifndef __ANIVA_PROC_CORE__
#define __ANIVA_PROC_CORE__

#include "dev/core.h"
#include "libk/async_ptr.h"
#include "libk/linkedlist.h"
#include "libk/error.h"
#include "libk/vector.h"
#include "libk/queue.h"

#define DEFAULT_STACK_SIZE                      (16 * Kib)
#define DEFAULT_THREAD_MAX_TICKS                (1)

#define PROC_DEFAULT_MAX_THREADS                (16)
#define PROC_CORE_PROCESS_NAME                  "[aniva-core]"
#define PROC_MAX_TICKS                          (4)

#define PROC_SOFTMAX                            (0x1000)

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
 * Allocate a page and map it into the page dir.
 * Params:
 * - thread: the thread we will relocate the stub for
 * - offset: the offset that the virtual base will have. every increment will
 *           increase the real virtual offset by offset * stub_size 
 * - custom_flags: flags for the kmem allocator
 * - page_flags: flags for the pages
 */
ErrorOrPtr relocate_thread_entry_stub(struct thread* thread, uintptr_t offset, uint32_t custom_flags, uint32_t page_flags);

ErrorOrPtr destroy_relocated_thread_entry_stub(struct thread* thread);

/*
 * Register a socket on the kernel socket chain
 */
ErrorOrPtr socket_register(struct threaded_socket* socket);

/*
 * unregister a socket from the kernel socket chain
 */
ErrorOrPtr socket_unregister(struct threaded_socket* socket);

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
ErrorOrPtr spawn_thread(char name[32], FuncPtr entry, uint64_t arg0);

/*
 * find a socket based on its port
 * TODO: validate port based on checksum?
 */
struct threaded_socket *find_registered_socket(uint32_t port);

struct proc* find_proc(const char* name);
struct thread* find_thread(struct proc* proc, uint64_t tid);

ErrorOrPtr proc_register(struct proc* proc);
ErrorOrPtr proc_unregister(char* name);

/*
 * send a data-packet to a port
 * returns a pointer to the response ptr (thus a double pointer)
 * if all goes well, otherwise a nullptr
 */
extern async_ptr_t** send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * Send a packet to the socket and discard the response buffer
 */
extern ErrorOrPtr send_packet_to_socket_no_response(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size); // socket.c

/*
 * same thing as the function above, but it includes a driver control code
 */
extern async_ptr_t** send_packet_to_socket_with_code(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size); // socket.c

/*
 * above function but blocking
 */
extern struct packet_response send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * validata a tspckt based on its identifier (checksum, hash, idk man)
 */
extern bool validate_tspckt(struct tspckt* packet); // tspctk.c

/*
 * Utilises CRC32 to generate a 32-bit checksum, based on the packet struct, its buffer, and its sender thread
 */
extern uint32_t generate_tspckt_identifier(struct tspckt* packet); // tspckt.c

#endif //__LIGHTHOUSE_OS_CORE__

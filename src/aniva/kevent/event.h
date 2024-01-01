#ifndef __ANIVA_KEVENT_EVENT__
#define __ANIVA_KEVENT_EVENT__

#include "proc/core.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/processor.h"
#include <libk/stddef.h>

/* Are any errors in the event chain fatal? */
#define KEVENT_FLAG_ERROR_FATAL (0x00000001)
/* This event is blocked from firing */
#define KEVENT_FLAG_FROZEN      (0x00000002)
/* 
 * Can't be removed 
 * NOTE: it's the job of the kernel to check wether a user- or driver created event
 * may have this flag. The event subsystem may disable allowing the creation and/or
 * setting of this flag
 * (TODO: create this subsystem manager (SSM))
 */
#define KEVENT_FLAG_DEFAULT     (0x00000004)

/* Maximum amount of different events that may be present on a system */
#define KEVENT_SOFTMAX_COUNT 4096
#define KEVENT_HOOK_SOFTMAX  4096

struct kevent;
struct kevent_hook;

/*
 * The type of event we're dealing with
 * Can be used to identify the type of buffer in the context
 */
enum KEVENT_TYPE {
  /* Keypresses and releases */
  KE_KEY_EVENT = 0,
  /* Any mouse movement */
  KE_MOUSE_EVENT,
  /* Device I/O, (dis)connects, ect. */
  KE_DEVICE_EVENT,
  /* Anything to do with processes (Crashes, launch, exit, ect.) */
  KE_PROC_EVENT,
  /* Anything with threads (Launch, exit, ext.) */
  KE_THREAD_EVENT,
  /* Profile events (Create, remove, (un)subscribes, ect.) */
  KE_PROFILE_EVENT,
  /* Any random crap */
  KE_GENERIC_EVENT,
  /* No clue */
  KE_UNKNOWN_EVENT,
  /* Something went wrong lmao */
  KE_ERROR_EVENT,
  /* Full system shutdown */
  KE_SHUTDOWN_EVENT,
  /* System slumber */
  KE_SLUMBER_EVENT,
  /* System sleep */
  KE_SLEEP_EVENT,
  /* Let us cook */
  KE_CUSTOM_EVENT,

  KE_EVENT_TYPE_COUNT,
};

/*
 * Context created when the event is fired
 */
typedef struct kevent_ctx {
  struct kevent* event;

  /* Events with a buffersize of over 4 Gib is unlikely I hope */
  uint32_t buffer_size;

  struct {
    bool in_irq:1;
    bool user_called:1;
  } flags;

  /* Info about the call origin */
  processor_t* orig_cpu;
  fid_t orig_fid;

  void* buffer;
} kevent_ctx_t;

int add_kevent(const char* name, enum KEVENT_TYPE type, uint32_t flags, uint32_t hook_capacity);

int remove_kevent(const char* name);
int remove_kevent_ex(struct kevent* event);

int freeze_kevent(const char* name);
int freeze_kevent_ex(struct kevent* event);

int unfreeze_kevent(const char* name);
int unfreeze_kevent_ex(struct kevent* event);

struct kevent* kevent_get(const char* name);

enum KEVENT_TYPE kevent_get_type(struct kevent* event);

int kevent_add_hook(const char* event_name, const char* hook_name, int (*f_hook)(kevent_ctx_t*));
int kevent_remove_hook(const char* event_name, const char* hook_name);

int kevent_add_hook_ex(struct kevent* event, const char* hook_name, int (*f_hook)(kevent_ctx_t*));
int kevent_remove_hook_ex(struct kevent* event, const char* hook_name);

struct kevent_hook* kevent_get_hook(struct kevent* event, const char* name);
struct kevent_hook* kevent_scan_hook(struct kevent* event, const char* name);

int kevent_fire(const char* name, void* buffer, size_t size);
int kevent_fire_ex(struct kevent* event, void* buffer, size_t size);

/* Kevent flag manipulation */
void kevent_flag_set(struct kevent* event, uint32_t flags);
void kevent_flag_unset(struct kevent* event, uint32_t flags);
bool kevent_flag_isset(struct kevent* event, uint32_t flags);


void init_kevents();

#endif // !__ANIVA_KEVENT_EVENT__

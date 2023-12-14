#ifndef __ANIVA_KEVENT_EVENT__
#define __ANIVA_KEVENT_EVENT__

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
 KE_KEY_EVENT = 0,
 KE_MOUSE_EVENT,
 KE_DEVICE_EVENT,
 KE_SW_EVENT,
 KE_GENERIC_EVENT,
 KE_UNKNOWN_EVENT,
 KE_ERROR_EVENT,
 KE_CUSTOM_EVENT,

 KE_EVENT_TYPE_COUNT,
};

/*
 * Context created when the event is fired
 */
typedef struct kevent_ctx {
  struct kevent* event;

  void* buffer;
  size_t buffer_size;
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

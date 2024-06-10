#ifndef __ANIVA_KEVENT_HOOK__
#define __ANIVA_KEVENT_HOOK__

#include "kevent/event.h"

typedef int (*f_hook_fn)(kevent_ctx_t* ctx, void* param);
typedef bool (*f_hook_should_fire)(kevent_ctx_t* ctx, void* param);

/*
 * TODO: Depending on the type of hook that gets created, the hook gets handled differently
 * when the kevent is fired
 */
#define KEVENT_HOOKTYPE_FUNC 0
#define KEVENT_HOOKTYPE_POLLABLE 1

/* Default maximum length of the poll queue */
#define KEVENT_HOOK_DEFAULT_MAX_POLL_DEPTH 16

/*
 * Exctention for kevent hooks when they are polled
 *
 * Pollable kevent hooks don't have callback funcitons for when they are fired, but rather
 * have these buffers that filled with the fire data of the kevent.
 */
typedef struct kevent_hook_poll_block {
    void* buffer;
    size_t bsize;
    kevent_ctx_t ctx;

    struct kevent_hook_poll_block* next;
} kevent_hook_poll_block_t;

void destroy_keventhook_poll_block(kevent_hook_poll_block_t* block);

/*
 * Memory block that indicates a hook to be called
 *
 */
typedef struct kevent_hook {
    const char* hookname;

    union {
        f_hook_fn f_hook;
        f_hook_should_fire f_should_fire;
    };

    bool is_frozen : 1;
    bool is_set : 1;
    uint8_t type;
    uint16_t max_poll_depth;
    /* Hashed hookname */
    uint32_t key;

    kevent_hook_poll_block_t* poll_blocks;
    void* param;

    /* Double linking for the traverse */
    struct kevent_hook* next;
    struct kevent_hook* prev;
} kevent_hook_t;

int init_keventhook(kevent_hook_t* hook, const char* name, uint8_t type, FuncPtr hook_fn, void* param);
void destroy_keventhook(kevent_hook_t* hook);

int keventhook_call(kevent_hook_t* hook, kevent_ctx_t* ctx);
int keventhook_dequeue_poll_block(kevent_hook_t* hook, kevent_hook_poll_block_t** block);

bool keventhook_is_set(kevent_hook_t* hook);

#endif //__ANIVA_KEVENT_HOOK__

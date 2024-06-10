#include "hook.h"
#include "kevent/hash.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include <libk/stddef.h>
#include <libk/string.h>

/*!
 * @brief: Initialize memory used by the hook
 */
int init_keventhook(kevent_hook_t* hook, const char* name, uint8_t type, FuncPtr hook_fn, void* param)
{
    if (!hook)
        return -1;

    /* Should be destroyed first */
    if (keventhook_is_set(hook))
        return -2;

    memset(hook, 0, sizeof(*hook));

    hook->key = kevent_compute_hook_key(name);
    hook->hookname = strdup(name);
    hook->f_hook = (f_hook_fn)hook_fn;
    hook->type = type;
    hook->param = param;
    hook->max_poll_depth = KEVENT_HOOK_DEFAULT_MAX_POLL_DEPTH;

    /* Make sure we know this hook is set */
    hook->is_set = true;

    return 0;
}

int keventhook_dequeue_poll_block(kevent_hook_t* hook, kevent_hook_poll_block_t** block)
{
    kevent_hook_poll_block_t* target_block;

    if (!hook || !block)
        return -KERR_INVAL;

    target_block = hook->poll_blocks;

    if (!target_block)
        return -KERR_NULL;

    /* Link into next */
    hook->poll_blocks = target_block->next;

    /* Return the right block */
    *block = target_block;
    return 0;
}

void destroy_keventhook_poll_block(kevent_hook_poll_block_t* block)
{
    if (!block)
        return;

    if (block->buffer)
        kfree(block->buffer);

    kzfree(block, sizeof(*block));
}

/*!
 * @brief: Clear memory used by the hook
 */
void destroy_keventhook(kevent_hook_t* hook)
{
    int error;
    kevent_hook_poll_block_t* c_block;

    kfree((void*)hook->hookname);

    /* Clear out all the poll blocks */
    do {
        c_block = NULL;

        error = keventhook_dequeue_poll_block(hook, &c_block);

        if (KERR_OK(error) && c_block)
            destroy_keventhook_poll_block(c_block);

    } while (KERR_OK(error) && c_block);

    memset(hook, 0, sizeof(*hook));
}

static inline int _enqueue_poll_block(kevent_hook_t* hook, kevent_ctx_t* ctx)
{
    uint32_t c_poll_depth;
    kevent_hook_poll_block_t** p_block;
    kevent_hook_poll_block_t* block;

    /* Get the poll blocks start */
    p_block = &hook->poll_blocks;
    c_poll_depth = 0;

    /* Walk the poll blocks */
    while (*p_block && c_poll_depth++ < hook->max_poll_depth)
        p_block = &(*p_block)->next;

    /* Already too many poll blocks */
    if (c_poll_depth >= hook->max_poll_depth || *p_block)
        return -1;

    block = kzalloc(sizeof(*block));

    if (!block)
        return -1;

    memset(block, 0, sizeof(*block));

    /* Copy the context field */
    memcpy(&block->ctx, ctx, sizeof(*ctx));

    /* Allocate a new buffer */
    block->bsize = block->ctx.buffer_size;
    block->buffer = kmalloc(block->bsize);

    /* Copy over the buffer data */
    memcpy(block->buffer, ctx->buffer, block->bsize);

    /* Make sure our internal context points to our buffer */
    block->ctx.buffer = block->buffer;

    /* Link into the hook */
    *p_block = block;
    return 0;
}

/*!
 * @brief: Root handler function for pollable kevent hooks are called
 *
 * This function is responsible for adding a new poll block and setting the hook to the correct state
 */
static int keventhook_call_pollable(kevent_hook_t* hook, kevent_ctx_t* ctx)
{
    /* Respect the firing condition of this hook, if it has one */
    if (hook->f_should_fire && !hook->f_should_fire(ctx, hook->param))
        return 0;

    return _enqueue_poll_block(hook, ctx);
}

/*!
 * @brief: Try to call the hooks function
 *
 * Returns -1 if the hook has no function
 */
int keventhook_call(kevent_hook_t* hook, kevent_ctx_t* ctx)
{
    if (!hook->f_hook)
        return -1;

    switch (hook->type) {
    case KEVENT_HOOKTYPE_FUNC:
        return hook->f_hook(ctx, hook->param);
    case KEVENT_HOOKTYPE_POLLABLE:
        return keventhook_call_pollable(hook, ctx);
    }

    return -1;
}

/*!
 * @brief: Check if a hook is used
 */
bool keventhook_is_set(kevent_hook_t* hook)
{
    return (hook->is_set || hook->hookname || hook->f_hook);
}

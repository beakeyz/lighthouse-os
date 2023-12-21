#include "keyboard.h"
#include <libk/string.h>

int init_kevent_kb_keybuffer(struct kevent_kb_keybuffer* out, kevent_kb_ctx_t* buffer, uint32_t capacity)
{
  if (!out)
    return -1;

  memset(out, 0, sizeof(*out));

  out->buffer = buffer;
  out->capacity = capacity;

  return 0;
}

kevent_kb_ctx_t* keybuffer_read_key(struct kevent_kb_keybuffer* buffer)
{
  kevent_kb_ctx_t* ret;

  if (!buffer || buffer->r_idx == buffer->w_idx)
    return nullptr;

  ret = &buffer->buffer[buffer->r_idx++];

  buffer->r_idx %= buffer->capacity;

  return ret;
}

int keybuffer_write_key(struct kevent_kb_keybuffer* buffer, kevent_kb_ctx_t* event)
{
  if (!buffer || !event)
    return -1;

  /* Write the event */
  memcpy(&buffer->buffer[buffer->w_idx], event, sizeof(*event));

  /* Cycle */
  buffer->w_idx++;
  buffer->w_idx %= buffer->capacity;

  return 0;
}

int keybuffer_clear(struct kevent_kb_keybuffer* buffer)
{
  if (!buffer || !buffer->buffer)
    return -1;

  memset(buffer->buffer, 0, buffer->capacity * sizeof(kevent_kb_ctx_t));
  return 0;
}

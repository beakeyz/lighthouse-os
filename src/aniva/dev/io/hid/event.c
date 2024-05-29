#include "hid.h"
#include "event.h"
#include "kevent/event.h"
#include "libk/data/queue.h"
#include "lightos/event/key.h"
#include "mem/zalloc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

static thread_t* _hid_event_thread;
static zone_allocator_t* _hid_event_zalloc;
static queue_t* _hid_event_queue;

kerror_t queue_hid_event(hid_event_t* event)
{
  if (!event)
    return -KERR_INVAL;

  if (!_hid_event_thread || !_hid_event_queue)
    return -KERR_INVAL;

  /* Enqueueu the event */
  queue_enqueue(_hid_event_queue, event);
  return 0;
}

bool hid_event_is_keycombination_pressed(hid_event_t* ctx, enum ANIVA_SCANCODES* keys, uint32_t len)
{
  if (ctx->type != HID_EVENT_KEYPRESS)
    return false;

  for (uint32_t i = 0; i < arrlen(ctx->key.pressed_keys) && len; i++) {
    /* Mismatch, return false */
    if (ctx->key.pressed_keys[i] != keys[i])
      return false;

    len--;
  }

  return true;
}

int init_hid_event_keybuffer(struct hid_event_keybuffer* out, hid_event_t* buffer, uint32_t capacity)
{
  if (!out)
    return -1;

  memset(out, 0, sizeof(*out));

  out->buffer = buffer;
  out->capacity = capacity;

  return 0;
}

hid_event_t* keybuffer_read_key(struct hid_event_keybuffer* buffer)
{
  hid_event_t* ret;

  if (!buffer || buffer->r_idx == buffer->w_idx)
    return nullptr;

  ret = &buffer->buffer[buffer->r_idx++];

  buffer->r_idx %= buffer->capacity;

  return ret;
}

int keybuffer_write_key(struct hid_event_keybuffer* buffer, hid_event_t* event)
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

int keybuffer_clear(struct hid_event_keybuffer* buffer)
{
  if (!buffer || !buffer->buffer)
    return -1;

  memset(buffer->buffer, 0, buffer->capacity * sizeof(hid_event_t));
  return 0;
}

hid_event_t* create_hid_event(struct hid_device* dev, enum HID_EVENT_TYPE type)
{
  hid_event_t* ret;

  ret = zalloc_fixed(_hid_event_zalloc);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->device = dev;
  ret->type = type;

  return ret;
}

void destroy_hid_event(hid_event_t* event)
{
  zfree_fixed(_hid_event_zalloc, event);
}

static void _hid_event_thread_fn(void)
{
  hid_event_t* event;

  do {
    event = queue_dequeue(_hid_event_queue);

    /* No events in the queue, block */
    if (!event) {
      scheduler_yield();
      continue;
    }

    /* Fire the event */
    kevent_fire("hid", event, sizeof(*event));

    /* Destroy the event object */
    destroy_hid_event(event);
  } while (true);
}

void init_hid_events()
{
  /* Register a kernel event */
  ASSERT_MSG(KERR_OK(add_kevent(HID_EVENTNAME, KE_DEVICE_EVENT, NULL, 512)), "Failed to add HID kevent");

  _hid_event_queue = create_limitless_queue();

  /* Create a zone allocator for our hid events */
  _hid_event_zalloc = create_zone_allocator(1 * Mib, sizeof(hid_event_t), NULL);

  /* Spawn a thread to distribute */
  _hid_event_thread = spawn_thread("hid_event_dispatcher", _hid_event_thread_fn, NULL);
}

#include "event.h"
#include "libk/flow/error.h"
#include <libk/string.h>

int init_hid_event_buffer(struct hid_event_buffer* out, hid_event_t* buffer, uint32_t capacity)
{
    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    out->buffer = buffer;
    out->capacity = capacity;

    return 0;
}

hid_event_t* hid_event_buffer_read(struct hid_event_buffer* buffer, uint32_t* p_r_idx)
{
    uint32_t r_idx;
    hid_event_t* ret;

    if (!p_r_idx)
        return nullptr;

    r_idx = *p_r_idx;

    if (!buffer || r_idx == buffer->w_idx)
        return nullptr;

    ret = &buffer->buffer[r_idx++];

    *p_r_idx = r_idx % buffer->capacity;
    return ret;
}

int hid_event_buffer_write(struct hid_event_buffer* buffer, hid_event_t* event)
{
    if (!buffer || !event)
        return -1;

    /* Write the event */
    memcpy(&buffer->buffer[buffer->w_idx++], event, sizeof(*event));

    /* Cycle */
    buffer->w_idx %= buffer->capacity;

    return 0;
}

int hid_event_buffer_clear(struct hid_event_buffer* buffer)
{
    if (!buffer || !buffer->buffer)
        return -1;

    memset(buffer->buffer, 0, buffer->capacity * sizeof(hid_event_t));

    buffer->r_idx = buffer->w_idx;
    return 0;
}

int hid_event_write_to_user(struct hid_event_buffer* buffer, uint32_t* p_r_idx, lwindow_t* uwnd)
{
    lkey_event_t* u_event;
    hid_event_t* k_event = hid_event_buffer_read(buffer, p_r_idx);

    if (!k_event)
        return -KERR_INVAL;

    u_event = &uwnd->keyevent_buffer[uwnd->keyevent_buffer_write_idx++];

    /* Write the data */
    u_event->keycode = k_event->key.scancode;
    u_event->pressed_char = k_event->key.pressed_char;
    u_event->pressed = (k_event->key.flags & HID_EVENT_KEY_FLAG_PRESSED) == HID_EVENT_KEY_FLAG_PRESSED;
    u_event->mod_flags = ((k_event->key.flags & HID_EVENT_KEY_FLAG_MOD_MASK) >> HID_EVENT_KEY_FLAG_MOD_BITSHIFT);

    /* Make sure we cycle the index */
    uwnd->keyevent_buffer_write_idx %= uwnd->keyevent_buffer_capacity;
    return 0;
}

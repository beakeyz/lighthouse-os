#ifndef __ANIVA_HIDDEVICE_EVENT__
#define __ANIVA_HIDDEVICE_EVENT__

#include "lightos/event/key.h"
#include <libk/stddef.h>

enum HID_EVENT_TYPE {
    HID_EVENT_KEYPRESS = 0,
    HID_EVENT_MOUSE,
    HID_EVENT_STATUS_CHANGE,
    HID_EVENT_CONNECT,
    HID_EVENT_DISCONNECT,
};

#define HID_EVENT_KEY_FLAG_PRESSED 0x01
#define HID_EVENT_KEY_FLAG_RESET 0x02

#define HID_EVENT_KEY_FLAG_MOD_ALT 0x04
#define HID_EVENT_KEY_FLAG_MOD_CTRL 0x08
#define HID_EVENT_KEY_FLAG_MOD_SHIFT 0x10
#define HID_EVENT_KEY_FLAG_MOD_SUPER 0x20
#define HID_EVENT_KEY_FLAG_MOD_ALTGR 0x40
#define HID_EVENT_KEY_FLAG_MOD_MASK 0x7C
#define HID_EVENT_KEY_FLAG_MOD_BITSHIFT 2

#define HID_MOUSE_FLAG_LBTN_PRESSED 0x0001
#define HID_MOUSE_FLAG_RBTN_PRESSED 0x0002
#define HID_MOUSE_FLAG_MBTN_PRESSED 0x0004

/*
 * Event that a HID device can generate
 */
typedef struct hid_event {
    enum HID_EVENT_TYPE type;

    union {
        /* Keyboard event */
        struct {
            enum ANIVA_SCANCODES scancode;
            uint8_t flags;
            char pressed_char;
            uint16_t pressed_buffer[7];
        } key;
        /* Mouse event */
        struct {
            int32_t deltax;
            int32_t deltay;
            int32_t deltaz;
            uint16_t flags;
        } mouse;
    };

    struct hid_device* device;
} hid_event_t;

static inline bool hid_event_is_keycombination_pressed(hid_event_t* event, enum ANIVA_SCANCODES* keys, uint32_t len)
{
    if (event->type != HID_EVENT_KEYPRESS)
        return false;

    /* This is a key release =/ */
    if (!(event->key.flags | HID_EVENT_KEY_FLAG_PRESSED))
        return false;

    for (uint32_t i = 0; i < arrlen(event->key.pressed_buffer) && len; i++) {
        /* Mismatch, return false */
        if (event->key.pressed_buffer[i] != keys[i])
            return false;

        len--;
    }

    return true;
}

static inline bool hid_keyevent_is_pressed(hid_event_t* event)
{
    if (event->type != HID_EVENT_KEYPRESS)
        return false;

    return ((event->key.flags & HID_EVENT_KEY_FLAG_PRESSED) == HID_EVENT_KEY_FLAG_PRESSED);
}

/**
 * @brief: Ringbuffer for our hid events
 */
typedef struct hid_event_buffer {
    uint32_t capacity;
    uint32_t w_idx;
    uint32_t r_idx;
    hid_event_t* buffer;
} hid_event_buffer_t;

int init_hid_event_buffer(struct hid_event_buffer* out, hid_event_t* buffer, uint32_t capacity);
hid_event_t* hid_event_buffer_read(struct hid_event_buffer* buffer, uint32_t* p_r_idx);
int hid_event_buffer_write(struct hid_event_buffer* buffer, hid_event_t* event);
int hid_event_buffer_clear(struct hid_event_buffer* buffer);

#endif // !__ANIVA_HIDDEVICE_EVENT__

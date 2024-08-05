#ifndef __LIGHTOS_LIGHTUI_WIDGET__
#define __LIGHTOS_LIGHTUI_WIDGET__

#include "lightos/lib/lightos.h"
#include "lightui/window.h"
#include <stdint.h>

/*
 * The core for our widget-based UI
 *
 * A lot of these concepts I've borrowed from light-loader, which already has some kind of widget-ish
 * interface stuff, which works pretty well
 */

enum LIGHTUI_WIDGET_TYPE {
    LIGHTUI_WIDGET_TYPE_BUTTON,
    LIGHTUI_WIDGET_TYPE_CHECKBOX,
    LIGHTUI_WIDGET_TYPE_SLIDER,
    LIGHTUI_WIDGET_TYPE_DROPDOWN,
    LIGHTUI_WIDGET_TYPE_SELECTOR,
    LIGHTUI_WIDGET_TYPE_INPUTFIELD,
    LIGHTUI_WIDGET_TYPE_TEXT,
    LIGHTUI_WIDGET_TYPE_IMAGE,
    LIGHTUI_WIDGET_TYPE_VIDEO,
    LIGHTUI_WIDGET_TYPE_CUSTOM,
};

enum LIGHTUI_WIDGETEVENT_TYPE {
    LIGHTUI_WIDGETEVENT_TYPE_CREATE,
    LIGHTUI_WIDGETEVENT_TYPE_DESTROY,
    LIGHTUI_WIDGETEVENT_TYPE_KEY,
    LIGHTUI_WIDGETEVENT_TYPE_CLICK,
    LIGHTUI_WIDGETEVENT_TYPE_SELECT,
};

#define LIGHTUI_WIDGETEVENT_FLAG_PRESSED 0x0001
#define LIGHTUI_WIDGETEVENT_FLAG_LMB 0x0002
#define LIGHTUI_WIDGETEVENT_FLAG_RMB 0x0004
#define LIGHTUI_WIDGETEVENT_FLAG_MMB 0x0008
#define LIGHTUI_WIDGETEVENT_FLAG_4MB 0x0010
#define LIGHTUI_WIDGETEVENT_FLAG_5MB 0x0020

typedef struct lightui_widget_event {
    enum LIGHTUI_WIDGETEVENT_TYPE type;
    uint16_t flags;
    uint16_t scancode;
} lightui_widget_event_t;

typedef struct lightui_widget {
    lightui_window_t* wnd;
    uint32_t x, y;
    uint32_t width, height;
    enum LIGHTUI_WIDGET_TYPE type;

    /* Label which is like an identifier */
    const char* label;
    /* Text-based content. May be null */
    const char* content;

    /* Private data which differs per widget type */
    void* private;

    /* GFX ops */
    void (*f_draw)(struct lightui_widget* w);
    void (*f_onevent)(struct lightui_widget* w, lightui_widget_event_t* event);

    /* Links widgets together */
    struct lightui_widget* next;
} lightui_widget_t;

/*!
 * @brief: Adds a generic widget to the window
 */
LEXPORT int lightui_window_add_widget(lightui_window_t* wnd, const char* label, const char* content, uint32_t x, uint32_t y, uint32_t width, uint32_t height, enum LIGHTUI_WIDGET_TYPE type);

#endif // !__LIGHTOS_LIGHTUI_WIDGET__

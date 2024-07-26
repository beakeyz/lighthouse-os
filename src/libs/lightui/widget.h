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

    /* Lifetime ops */
    void (*f_create)(struct lightui_widget* w);
    void (*f_destroy)(struct lightui_widget* w);

    /* GFX ops */
    void (*f_draw)(struct lightui_widget* w);
    void (*f_onclick)(struct lightui_widget* w);
    void (*f_onkey)(struct lightui_widget* w);
    void (*f_onselect)(struct lightui_widget* w);

    /* Links widgets together */
    struct lightui_widget* next;
} lightui_widget_t;

/*!
 * @brief: Adds a generic widget to the window
 */
LEXPORT int lightui_window_add_widget(lightui_window_t* wnd, const char* label, const char* content, uint32_t x, uint32_t y, uint32_t width, uint32_t height, enum LIGHTUI_WIDGET_TYPE type);

#endif // !__LIGHTOS_LIGHTUI_WIDGET__

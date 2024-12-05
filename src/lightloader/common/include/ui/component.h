#ifndef __LIGHTLOADER_COMPONENT__
#define __LIGHTLOADER_COMPONENT__

#include "gfx.h"
#include "key.h"
#include "mouse.h"
#include <stdint.h>

struct light_font;
struct light_gfx;

#define COMPONENT_TYPE_BOX (0)
#define COMPONENT_TYPE_BUTTON (1)
#define COMPONENT_TYPE_LABEL (2)
#define COMPONENT_TYPE_SWITCH (3)
#define COMPONENT_TYPE_INPUTBOX (4)
#define COMPONENT_TYPE_IMAGE (5)
#define COMPONENT_TYPE_LINK (6)

typedef struct light_component {
    char* label;
    uint8_t type;
    uint8_t should_update;
    uint32_t x, y;
    uint32_t width, height;

    void* private;

    struct light_gfx* gfx;
    struct light_font* font;

    light_mousepos_t* mouse_buffer;
    light_key_t* key_buffer;

    int (*f_ondraw)(struct light_component* this);
    bool (*f_should_update)(struct light_component* this);

    /* A component (like a group-box) might have child component. These are rendered relative to the parent */
    struct light_component* children;

    /* All ui components will be together in a linked list, so we can draw them from top to bottom */
    struct light_component* next;
} light_component_t;

#define FOREACH_UI_COMPONENT(i, list) for (light_component_t* i = list; i; i = i->next)

light_component_t* create_component(light_component_t** link, uint8_t type, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*ondraw)(struct light_component*));
int component_draw_string_at(light_component_t* component, char* str, uint32_t x, uint32_t y, light_color_t clr);

light_component_t* get_bottom_component(light_component_t** link);

void draw_component(light_component_t* component, light_key_t key, light_mousepos_t mouse);
void update_component(light_component_t* component, light_key_t key, light_mousepos_t mouse);

static inline bool
component_is_hovered(light_component_t* component)
{
    return (
        component->mouse_buffer && component->mouse_buffer->x >= component->x && component->mouse_buffer->x < component->x + component->width && component->mouse_buffer->y >= component->y && component->mouse_buffer->y < component->y + component->height);
}

#endif // !__LIGHTLOADER_COMPONENT__

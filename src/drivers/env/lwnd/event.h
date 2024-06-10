#ifndef __ANIVA_LWND_EVENT__
#define __ANIVA_LWND_EVENT__

#include "drivers/env/lwnd/window.h"
#include <libk/stddef.h>

struct lwnd_screen;

#define LWND_EVENT_TYPE_MOVE 0
#define LWND_EVENT_TYPE_RESIZE 1

typedef struct lwnd_event {

    uint8_t type;
    window_id_t window_id;

    union {
        struct {
            uint32_t new_x;
            uint32_t new_y;
        } move;
        struct {
            uint32_t new_width;
            uint32_t new_height;
        } resize;
    };

    struct lwnd_event* next;
} lwnd_event_t;

lwnd_event_t* create_lwnd_move_event(window_id_t window_id, uint32_t new_x, uint32_t new_y);
void destroy_lwnd_event(lwnd_event_t* event);

#endif // !__ANIVA_LWND_EVENT__

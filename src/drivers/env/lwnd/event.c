#include "event.h"
#include "libk/stddef.h"
#include "mem/heap.h"

lwnd_event_t* create_lwnd_move_event(window_id_t window_id, uint32_t new_x, uint32_t new_y)
{
    lwnd_event_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->window_id = window_id;
    ret->move.new_x = new_x;
    ret->move.new_y = new_y;

    ret->type = LWND_EVENT_TYPE_MOVE;

    return ret;
}

void destroy_lwnd_event(lwnd_event_t* event)
{
    kfree(event);
}

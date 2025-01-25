#ifndef __LIGHTOS_LWND_API_H__
#define __LIGHTOS_LWND_API_H__

enum LWND_PT {
    LWND_PT_CREATE_WND,
    LWND_PT_DESTROY_WND,
    LWND_PT_MOVE_WND,
    LWND_PT_RESIZE_WND,
    LWND_PT_MINIMIZE_WND,
    LWND_PT_MAXIMIZE_WND,
    LWND_PT_MOUSE_CLICK,
    LWND_PT_KB_CLICK,
    LWND_PT_UNFOCUS,
};

typedef struct lwindow_packet {

} lwindow_packet_t;

#endif // !__LIGHTOS_LWND_API_H__

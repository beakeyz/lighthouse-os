#ifndef __LIGHTOS_KTERM_ULIB_SHARED__
#define __LIGHTOS_KTERM_ULIB_SHARED__

typedef struct kterm_box_constr {
    unsigned int* p_id;
    unsigned int x, y;
    unsigned char color;
    const char title[47];
    const char content[192];
} kterm_box_constr_t;

enum KTERM_DRV_CTLC {
    KTERM_DRV_GET_CWD,
    KTERM_DRV_CLEAR,
    KTERM_DRV_CREATE_BOX,
    KTERM_DRV_UPDATE_BOX,
    KTERM_DRV_REMOVE_BOX,
};

#endif // !__LIGHTOS_KTERM_ULIB_SHARED__

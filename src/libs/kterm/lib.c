#include "kterm/shared.h"
#include "lightos/driver/drv.h"
#include "lightos/lib/lightos.h"
#include "string.h"
#include <errno.h>
#include <stdio.h>

static HANDLE kterm_handle;

/*!
 * @brief: Get the CWD directly from kterm
 */
int kterm_get_cwd(const char** cwd)
{
    /* Allocate a static buffer on the stack */
    const char buffer[256] = { 0 };

    /* Do a lil ioctl */
    if (!driver_send_msg(kterm_handle, KTERM_DRV_GET_CWD, (void*)buffer, sizeof(buffer)))
        return -1;

    /* Copy the shit */
    *cwd = strdup(buffer);
    return 0;
}

int kterm_update_box(uint32_t p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content)
{
    uint32_t title_len, content_len;

    if (!title || !content)
        return -EINVAL;

    title_len = strlen(title);
    content_len = strlen(content);

    kterm_box_constr_t constr = {
        .x = x,
        .y = y,
        .w = 0,
        .h = 0,
        .color = color_idx,
        .title = { 0 },
        .content = { 0 },
        .p_id = &p_id,
    };

    if (title_len >= sizeof(constr.title))
        title_len = sizeof(constr.title) - 1;

    if (content_len >= sizeof(constr.content))
        content_len = sizeof(constr.content) - 1;

    memcpy((void*)&constr.title, title, title_len);
    memcpy((void*)&constr.content, content, content_len);

    /* Content will occupy a single line */
    constr.h = 1;
    /* Make the box as long ast the content_len is. One unit width corresponds with one character */
    constr.w = content_len + 2;

    /* Send the driver the message */
    driver_send_msg(kterm_handle, KTERM_DRV_UPDATE_BOX, &constr, sizeof(constr));

    return 0;
}

/*!
 * @brief: Entrypoint for the kterm userkit
 *
 * Checks if kterm is loaded
 */
LIGHTENTRY int kterm_ulib_entry()
{
    BOOL res;

    res = open_driver("other/kterm", NULL, NULL, &kterm_handle);

    /* Could not open the driver, fuck */
    if (!res)
        return -1;

    return 0;
}

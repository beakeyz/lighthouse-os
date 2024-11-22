#ifndef __LIGHTOS_KTERM_ULIB__
#define __LIGHTOS_KTERM_ULIB__

/*!
 * @brief: Ask kterm what it's CWD is
 */

#include <stdint.h>

extern int kterm_get_cwd(const char** cwd);
extern int kterm_update_box(uint32_t p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content);

#endif // !__LIGHTOS_KTERM_ULIB__

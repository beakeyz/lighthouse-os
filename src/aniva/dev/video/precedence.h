#ifndef __ANIVA_VID_DEV_PRECEDENCE__
#define __ANIVA_VID_DEV_PRECEDENCE__

#include <libk/stddef.h>

/*
 * Since there may only be one active graphics driver, we need to be able
 * to judge which driver we'd rather load. A driver may supply it's own 
 * precedence value, which we will check against the current loaded graphics 
 * driver. If its higher, we will replace it with the current driver.
 *
 * How do we know the driver isn't talking bs? Well if the driver is claiming to
 * have extreme precedence, but also does not have a msg function for driver control codes,
 * we can quickly assume its lying. Other integrity checks can filter out big bullcrap.
 * However we don't go further than that, so we simply trust drivers to report 
 * the correct value. If they don't, that's not really our problem lmao
 */
typedef uint8_t video_dev_precedence_t;

#define VID_DEV_PRECEDENCE_BASIC    10
#define VID_DEV_PRECEDENCE_MID      50
#define VID_DEV_PRECEDENCE_HIGH     100
#define VID_DEV_PRECEDENCE_EXTREME  200
#define VID_DEV_PRECEDENCE_FULL     0xff

#endif // !__ANIVA_VID_DEV_PRECEDENCE__

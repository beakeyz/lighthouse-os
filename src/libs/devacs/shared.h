#ifndef __LIGHTOS_DEVACS_SHARED_STRUCTS__
#define __LIGHTOS_DEVACS_SHARED_STRUCTS__

#include <stdint.h>

/*
 * Enum with low level connection types of a device
 *
 * This enum describes how the host talks to a particular device
 */
enum DEVICE_CTYPE {
    DEVICE_CTYPE_PCI,
    DEVICE_CTYPE_USB,
    DEVICE_CTYPE_AHCI,
    DEVICE_CTYPE_IDE,
    DEVICE_CTYPE_ATAPI,
    DEVICE_CTYPE_SOFTDEV,
    DEVICE_CTYPE_CPU,
    DEVICE_CTYPE_I2C,
    DEVICE_CTYPE_GPIO,

    DEVICE_CTYPE_UNKNOWN,
    DEVICE_CTYPE_OTHER
};

/*
 * Data block which a device fills when there is
 * a query for the deviceinfo
 */
typedef struct devinfo {
    const char devicename[64];
    const char manufacturer[48];

    /* Describe the origins of this devce */
    uint16_t vendorid;
    uint16_t deviceid;
    uint16_t class;
    uint16_t subclass;

    /* Some higher level description(s) */
    enum DEVICE_CTYPE ctype;

    /* Device specific data */
    uint32_t dev_specific_size;
    uint8_t dev_specific_info[];
} devinfo_t, DEVINFO;

#endif // !__LIGHTOS_DEVACS_SHARED_STRUCTS__

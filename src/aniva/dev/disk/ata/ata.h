#ifndef __ANIVA_ATA__
#define __ANIVA_ATA__
#include <dev/driver.h>

/*
 * Default generic ata driver for the kernel.
 */

typedef struct ata_port {
} ata_port_t;

typedef struct ata_dev {

} ata_dev_t;

const extern aniva_driver_t g_base_ata_driver;

#endif // !__ANIVA_ATA__

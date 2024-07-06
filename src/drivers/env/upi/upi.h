#ifndef __ANIVA_DRIVERS_UPI_PRIVATE__
#define __ANIVA_DRIVERS_UPI_PRIVATE__

#include <proc/proc.h>

/*!
 * @brief: Kernel-side pipe structure
 *
 *
 */
typedef struct upi_pipe {
    proc_t* creator_proc;
} upi_pipe_t;

#endif // !__ANIVA_DRIVERS_UPI_PRIVATE__

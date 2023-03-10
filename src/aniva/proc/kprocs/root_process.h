#ifndef __ANIVA_ROOT_PROCESS__ 
#define __ANIVA_ROOT_PROCESS__
#include <libk/stddef.h>
/*
 * The root process is responsible for a few things:
 *  - passing packets to the right sockets
 *  - supplying idle functionality
 *  - TODO ...
 */

void create_and_register_root_process(uintptr_t multiboot_addr);

#endif // !

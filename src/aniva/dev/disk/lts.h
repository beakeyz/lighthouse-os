#ifndef __ANIVA_DEV_LTS__ 
#define __ANIVA_DEV_LTS__

/*
 * The LTS subsystem groups all 'disk' devices (For us, devices which implement the DISK endpoint) together
 * in a single group. It's comparable to the way Linux stores disk devices at /dev and how on Windows we can
 * find 'volumes' by looking at \\\.\VolumeX (Or something like that idk Windows).
 *
 * TODO: ???
 */

void init_lts();



#endif // !

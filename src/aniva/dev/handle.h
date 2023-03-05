#ifndef __ANIVA_DEV_HANDLE__ 
#define __ANIVA_DEV_HANDLE__

/*
 * NOTE: a handle should be something that you want to 
 * rely on, but not necicarily change...
 * we want to be able to get a copy of the drivers data
 * using the handle, which is just some value whith which the kernel
 * can find the address of the driver
 */

typedef void* handle_t;

#endif // !

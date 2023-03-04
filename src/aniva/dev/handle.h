#ifndef __ANIVA_DEV_HANDLE__ 
#define __ANIVA_DEV_HANDLE__

/*
 * NOTE: a handle should be something that you want to 
 * rely on, but not necicarily change...
 */

typedef void* handle_t;

handle_t get_module_handle();

#endif // !

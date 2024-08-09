#ifndef __ANIVA_KPROC_IDLE__
#define __ANIVA_KPROC_IDLE__

struct proc;
struct thread;

void init_kernel_idle();
void __generic_proc_idle();

extern struct proc* __kernel_idle_proc;
extern struct thread* __kernel_idle_thread;

#endif // !__ANIVA_KPROC_IDLE__

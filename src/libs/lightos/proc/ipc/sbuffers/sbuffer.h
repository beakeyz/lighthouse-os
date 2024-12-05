#ifndef __LIGHTOS_SHARED_BUFFERS__
#define __LIGHTOS_SHARED_BUFFERS__

/*
 * Yayy another new weird subsystem that might or might not get used 0.0
 * This is our take on managed shared memory region between processes, which is pretty much the quickest
 * way of doing ipc
 *
 * We probably do want to use this in the window management process, audio mixer, input arbiter, ect.
 * these processes can just present certain shared regions with predetermined layouts, which other
 * processes can map into their own addressspace.
 *
 * TODO: Define and implement
 */

#endif // !__LIGHTOS_SHARED_BUFFERS__

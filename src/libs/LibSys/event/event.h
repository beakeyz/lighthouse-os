#ifndef __LIGHTENV_LIBSYS_EVENT__
#define __LIGHTENV_LIBSYS_EVENT__

/*
 * Userspace event interface
 *
 * How do events work?
 *  Lots of things can trigger events. There are three types of events under Aniva:
 *    1) Pure kernel events: these events are private to the kernel and kernel drivers
 *    2) Public events: these events are global for everyone to see
 *    3) Limited events: these events are only visible for the kernel and for processes that have been granted special
 *       access (Like their profile being elevated, or them being subscribed to a profile with access perms to this event)
 *  Userspace can query these events by calling event_poll on an event handle. If userspace owns this event, it can send 
 *  events by calling event_send on its event handle. Since most of the event stuff is handles upstream in the kernel, we
 *  need to call to a handle every time we want updates about our events.
 */

#endif // !__LIGHTENV_LIBSYS_EVENT__

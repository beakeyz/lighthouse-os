# The LightOS c library

This is the main system API library that represents most of the foundation of userspace.
Here we support IO, syscalls and different utilities for applications, so they can access the devices hardware in a structured, isolated and consistent way.

We've made this part of libc, because this is the layer beyond libc. This means any app that just
wants to do it's jolly thing can just use regular libc, which will talk to this, while anything
that wants to use specific lightos features does not have to rely on any other weird libraries, 
because everything is nicely baked into one monolithic entity.

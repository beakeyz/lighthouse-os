# The lighthouse-os userspace directory

This file serves as an introduction to the structure of this directory. It explains the layout, purpose, practises, ect.

## Layout

For now, every subdirectory in this directory represents a single userspace application. The name of this subdirectory must be the same
as the name of the application, otherwise the ramdisk/rootdisk manager will get confused. In order to include an application into the build
procedure, the directory must be added to the USER_PATHS variable inside the makefile present in this directory.

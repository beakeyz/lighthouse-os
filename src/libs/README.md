# LightOS userspace libraries

There are two types of userspace libraries:

1) Static binaries
2) Shared binaries

Shared binaries are the preferred way to have a library, since it saves space on the system, but static binaries are more
stable, since there are less points of failure. With static libraries, you simply add the binaries of the libraries directly
into the executable, whereas with shared binaries we tell the system which binaries it needs to load for us at runtime.

## Loading libraries

When a library is used by a program, it might need to setup some of it's local stuff for it's functionality to work. This is very
easy for shared libraries, since they simply specify which function is their initialization function by giving it the LIGHTENTRY prefix.
This tells the compiler to put the function into it's own section, which the dynamic loader will look for.

NOTE: It is possible to put multiple functions in this section, but the order of execution is not guaranteed. ABI (trust me) states that there should
only be one function present inside this section, which then determines further execution flow predictably. 

Right before transfering execution to the main program entry, all the dynamic dependencies are initialized by the method described above, with
libc being loaded first. After that we analyse the dependencies to determine the correct order of loading the libraries. After that we assume 
all is well and pray the application does not crash lol

## Needed libraries

- Network
- Disk
- USB
- GPU
- Prism bytecode
- Screen
- Firmware
- OpenGL

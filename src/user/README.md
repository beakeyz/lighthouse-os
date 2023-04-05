# The lighthouse-os userspace directory

This file serves as an introduction to the structure of this directory. It explains the layout, purpose, practises, ect.

## Layout

This directory is indexed by directory. Every userspace program is located in its own directory. The python build system will 
for this reason not look into this directory on its own. Every directory, i.e. userspace program (as I will refer to it from now on),
will generate it's own binary in out/user/binaries. Depending on if this binary is statically linked or not, we might have 
to change the layout a bit, we'll have to see about that later.

I want to have our build system confirm the integrity of every program that is located in this directory (src/user). This means I
want to make every program have it's own **manifest.json** file, where the information about the program is laid out for the 
build system, which will also provide you with utilities to generate a template.

Please note that the features mentioned above are still TODO. This merely serves as a little markup for the design I want to enroll.

## Purpose

The purpose of this approach is to make the development of preloaded software in the kernel as easy as possible. I want to 
eventually have an integrated build system with which the kernel can interface, in order to make program development possible
completely inside the operating system (before this is implemented, every userspace program will have to be compiled outside 
of the kernel and then be compressed and moved into an initial filesystem, where the kernel can load it at runtime)

## Practises

As stated in the first section, we want to have programmes laid out as individual components, with their own needs. These needs
will be disclosed in the aforementioned **manifest.json**, and they will be provided by the build system. 

# Disclaimer

This document is not finished and also not designed to be used by anyone who is not either contributing to the build system, or the
userspace itself. It is merely for me to conceptualise and ultimately immortalize my idea of how this system may come to be.

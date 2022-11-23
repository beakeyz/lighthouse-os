# TODO: refactor this directory

I want to have the layout of this directory be a make project for every different binary that should get built.
For example there will be a separate directory for: the kernel, libc, modules, ect.

Every one of these directories will hold their own Makefile to handle the build process of that binary. We will 
then have a central Makefile to oversee al this crap

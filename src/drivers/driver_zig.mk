#
# Template for building any driver in this directory
# Simply gets called from the main Makefile with 'make -f driver_zig.mk'
#
# This is possible because all drivers have a uniform structure: they're all relocatable
# objects that get linked directly against the kernel
#
# The following variables are exported from the underlying makefile:
# DRIVER_NAME: The rule is that any driver that gets built will get the 
#			   name of the directory its in
# THIS_OUT: The output directory for this driver
# THIS_SRC: The source directory for this driver
#

DIRECTORY_GUARD=mkdir -p $(@D)

ZIG_BIN := zig
ZIG_BUILDFLAGS := \
 -fPIC -O ReleaseSafe -mno-red-zone -fno-stack-protector -fno-stack-check --subsystem native \
 -fstrip -fno-error-tracing -fno-entry -fno-soname -mcmodel=large -ofmt=elf -static \
 -femit-bin=$(THIS_OUT)/$(DRIVER_NAME).drv \
 --zig-lib-dir $(ZIG_DIR)


ZIG_SRC := $(shell find $(THIS_SRC) -type f -name 'main.zig')

build: 
	$(DIRECTORY_GUARD) $(THIS_OUT)
	$(ZIG_BIN) build-obj $(ZIG_BUILDFLAGS) $(ZIG_SRC)

DIRECTORY_GUARD=mkdir -p $(@D)

INSTALL_DEV ?= none

define DEFAULT_VAR =
    ifeq ($(origin $1),default)
        override $(1) := $(2)
    endif
    ifeq ($(origin $1),undefined)
        override $(1) := $(2)
    endif
endef

# This bitch MUST not fail. If it does we're probably screwed
override WORKING_DIR=$(shell pwd)
override CROSSCOMPILER_BIN_DIR=$(WORKING_DIR)/cross_compiler/bin
export OUT=$(WORKING_DIR)/out
export SRC=$(WORKING_DIR)/src
export PROJECT_DIR=$(WORKING_DIR)/project
export TOOLS_DIR=$(WORKING_DIR)/tools
export SYSROOT_DIR=$(WORKING_DIR)/system
export SYSROOT_LIBRARY_DIR=$(SYSROOT_DIR)/System/Lib

export LIBRARY_BIN_PATH=$(WORKING_DIR)/out/libs

export EMU=qemu-system-x86_64
export CC=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-gcc
export LD=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-ld
export OBJCOPY=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-objcopy
export NM=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-nm
# NASM should be in PATH
export ASM=nasm

export KERNEL_NAME=aniva
export RAMDISK_NAME=nvrdisk
export KERNEL_FILENAME=$(KERNEL_NAME).elf
export RAMDISK_FILENAME=$(RAMDISK_NAME).igz

# Name defs
export LOADER_ELF=lightloader.elf
export LOADER_EFI=LIGHTLOADER.EFI

# Define lightloader directories
export LOADER_OUT=$(OUT)/lightloader
export LOADER_SRC=$(SRC)/lightloader


# 1) verbose
# 2) debug
# 3) info
# 4) none
export KERNEL_DEBUG_LEVEL=verbose
# yes = The kernel image will contain a list of tests that need to be done
#    	before userspace is launched
# no = The kernel will boot without doing tests
export KERNEL_COMPILE_TESTS=yes

# NOTE: we've removed -Wall, since it does not play nice with ACPICA
export KERNEL_CFLAGS := \
	-std=gnu11 -Werror -nostdlib -O2 -mno-sse -mno-sse2 -static \
	-mno-mmx -mno-80387 -mno-red-zone -m64 -march=x86-64 -mcmodel=large \
	-ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
	-fno-lto -fno-exceptions -MMD -I$(SRC) -I$(SRC)/$(KERNEL_NAME) -I$(SRC)/libs -I$(SRC)/libs/lightos/libc -I$(SRC)/$(KERNEL_NAME)/libk \
	-I$(SRC)/modules -D'ANIVA_KERNEL'

ifeq ($(KERNEL_DEBUG_LEVEL), verbose)
	KERNEL_CFLAGS += -DDBG_VERBOSE=1
endif
ifeq ($(KERNEL_DEBUG_LEVEL), debug)
	KERNEL_CFLAGS += -DDBG_DEBUG=1
endif
ifeq ($(KERNEL_DEBUG_LEVEL), info)
	KERNEL_CFLAGS += -DDBG_INFO=1
endif
ifeq ($(KERNEL_DEBUG_LEVEL), NONE)
	KERNEL_CFLAGS += -DDBG_NONE=1
endif

ifeq ($(KERNEL_COMPILE_TESTS), yes)
	KERNEL_CFLAGS += -DANIVA_COMPILE_TESTS=1
else
	KERNEL_CFLAGS += -DANIVA_COMPILE_TESTS=0
endif

export USER_INCLUDE_CFLAGS := \
	-I. -I$(SRC)/libs -I$(SRC)/libs/lightos/libc

export KERNEL_LDFLAGS := \
	-T ${KERNEL_LINKERSCRIPT_PATH} -export-dynamic -z max-page-size=0x1000

help: ## Prints help for targets with comments
	@cat $(MAKEFILE_LIST) | grep -E '^[a-zA-Z_-]+:.*?## .*$$' | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

#
# Currently these targets only invoke build targets, but idealy we
# also want to be able to invoke other targets, like clean, install, ect.
#

aniva: ## Build the kernel binary
	@make -j$(nproc) -C ./src/aniva build

drivers: ## Build on-disk drivers
	@make -C ./src/drivers build

libs: ## Build system libraries
	@make -C ./src/libs build

user: ## Build userspace stuff
	@make -C ./src/user build

modules: ## Build the shared system modules (modules.obj)
	@make -C ./src/modules build

loader: ## Build the bootloader sources
	@make -C ./src/lightloader build

ramdisk: ## Create the system ramdisk
	@echo -e "TODO: create ramdisk"
	@cd $(PROJECT_DIR)/.. && python3 $(PROJECT_DIR)/x.py ramdisk

clean: ## Remove any build artifacts
	@echo -e "Trying to clean build artifacts..."
	@rm -r $(OUT)
	@echo -e "Cleaned build artifacts!"

all: modules loader aniva drivers libs user ramdisk
	@echo Built the entire project =D

LIGHTOS_IMG=lightos.img
BOOTRT_DIR=$(OUT)/__bootrt
LOOPBACK_DEV=$(OUT)/loopback_dev

$(OUT)/$(LIGHTOS_IMG):
	@rm -f $@
	@dd if=/dev/zero of=$@ iflag=fullblock bs=1M count=64 && sync

image: $(OUT)/$(LIGHTOS_IMG)
	@sudo rm -rf $(BOOTRT_DIR)
	@sudo rm -rf $(LOOPBACK_DEV)
	@mkdir -p $(BOOTRT_DIR)
	@echo -e [Image] Setting up disk image...
	@sudo losetup -Pf --show $(OUT)/$(LIGHTOS_IMG) > $(LOOPBACK_DEV)
	@sudo parted `cat $(LOOPBACK_DEV)` mklabel gpt
	@sudo parted `cat $(LOOPBACK_DEV)` mkpart primary 2048s 100% >/dev/null
	@sudo parted `cat $(LOOPBACK_DEV)` set 1 boot on >/dev/null
	@sudo parted `cat $(LOOPBACK_DEV)` set 1 hidden on >/dev/null
	@sudo parted `cat $(LOOPBACK_DEV)` set 1 esp on >/dev/null
	@sudo partprobe `cat $(LOOPBACK_DEV)` >/dev/null # Sync >

	@echo -e [Image] Creating filesystem...
	@sudo mkfs.fat -F 32 `cat $(LOOPBACK_DEV)`p1 >/dev/null
	@sync
	@sudo mount `cat $(LOOPBACK_DEV)`p1 $(BOOTRT_DIR)

	@echo -e [Image] Copying filesystems...
	@sudo mkdir -p $(BOOTRT_DIR)/EFI/BOOT
	@sudo cp $(LOADER_OUT)/$(LOADER_EFI) $(BOOTRT_DIR)/EFI/BOOT/BOOTX64.EFI # Copy bootloader binary
	@sudo cp $(OUT)/$(KERNEL_FILENAME) $(BOOTRT_DIR) # Copy the kernel
	@sudo cp $(OUT)/$(RAMDISK_FILENAME) $(BOOTRT_DIR) # Copy the ramdisk
	@sudo cp -r $(SYSROOT_DIR)/* $(BOOTRT_DIR) # Copy the rest of the filesystem

	@echo -e [Image] Cleaning up...
	@sync
	@sudo umount $(BOOTRT_DIR)
	@sudo losetup -d `cat $(LOOPBACK_DEV)`
	@rm -rf $(BOOTRT_DIR)
	@rm -rf $(LOOPBACK_DEV)
	@echo -e [Image] Done!

debug: image
	@echo Debugging: Running system in QEMU
	@$(EMU) -m 1G -enable-kvm -net none -M q35 -usb $(OUT)/$(LIGHTOS_IMG) -bios ./ovmf/OVMF.fd -serial stdio -device usb-ehci -device usb-kbd -device usb-mouse
	# @$(EMU) -m 1G -enable-kvm -net none -M q35 -usb $(OUT)/$(LIGHTOS_IMG) -bios ./ovmf/OVMF.fd -serial stdio

install-gcc-hdrs:
	@$(MAKE) -C ./cross_compiler/build/binutils install
	@$(MAKE) -C ./cross_compiler/build/gcc install-gcc
	@$(MAKE) -C ./cross_compiler/build/gcc install-target-libgcc

.PHONY: install
install: image ## Install the bootloader onto a blockdevice (parameter INSTALL_DEV=<device_path>)
ifeq ($(INSTALL_DEV),none)
	@echo Please specify INSTALL_DEV=?
else
	@stat $(INSTALL_DEV)
	@sudo dd bs=4M if=$(OUT)/$(LIGHTOS_IMG) of=$(INSTALL_DEV) conv=fsync oflag=direct status=progress
endif

.PHONY: aniva drivers loader modules libs user ramdisk clean all install-gcc-hdrs

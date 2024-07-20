DIRECTORY_GUARD=mkdir -p $(@D)

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

export LIBRARY_BIN_PATH=$(WORKING_DIR)/out/libs

export CC=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-gcc
export LD=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-ld
export OBJCOPY=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-objcopy
export NM=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-nm
# NASM should be in PATH
export ASM=nasm

export KERNEL_NAME=aniva
export RAMDISK_NAME=anivaRamdisk
export KERNEL_FILENAME=$(KERNEL_NAME).elf
export RAMDISK_FILENAME=$(RAMDISK_NAME).igz
# 1) verbose
# 2) debug
# 3) info
# 4) none
export KERNEL_DEBUG_LEVEL=verbose

# NOTE: we've removed -Wall, since it does not play nice with ACPICA
export KERNEL_CFLAGS := \
	-std=gnu11 -Werror -nostdlib -O2 -mno-sse -mno-sse2 -static \
	-mno-mmx -mno-80387 -mno-red-zone -m64 -march=x86-64 -mcmodel=large \
	-ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
	-fno-lto -fno-exceptions -MMD -I$(SRC) -I$(SRC)/$(KERNEL_NAME) -I$(SRC)/libs -I$(SRC)/$(KERNEL_NAME)/libk \
	-I$(SRC)/libs/libc -D'KERNEL'

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

export USER_INCLUDE_CFLAGS := \
	-I. -I$(SRC)/libs -I$(SRC)/libs/libc

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
	@make -j$(nproc) -C ./src/drivers build

libs: ## Build system libraries
	@make -j$(nproc) -C ./src/libs build

user: ## Build userspace stuff
	@make -j$(nproc) -C ./src/user build

ramdisk: ## Create the system ramdisk
	@echo -e "TODO: create ramdisk"
	@cd $(PROJECT_DIR)/.. && python3 $(PROJECT_DIR)/x.py ramdisk

clean: ## Remove any build artifacts
	@echo -e "Trying to clean build artifacts..."
	@rm -r $(OUT)
	@echo -e "Cleaned build artifacts!"

all: aniva drivers libs user ramdisk
	@echo Built the entire project =D

install-gcc-hdrs:
	@$(MAKE) -C ./cross_compiler/build/binutils install
	@$(MAKE) -C ./cross_compiler/build/gcc install-gcc
	@$(MAKE) -C ./cross_compiler/build/gcc install-target-libgcc

.PHONY: aniva drivers libs user ramdisk clean all install-gcc-hdrs

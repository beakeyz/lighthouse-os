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

export CC=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-gcc
export LD=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-ld
export OBJCOPY=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-objcopy
export NM=$(CROSSCOMPILER_BIN_DIR)/x86_64-pc-lightos-nm
# NASM should be in PATH
export ASM=nasm

export KERNEL_NAME=aniva
export RAMDISK_NAME=rdisk
export KERNEL_FILENAME=$(KERNEL_NAME).elf
export RAMDISK_FILENAME=$(RAMDISK_NAME).igz

help: ## Prints help for targets with comments
	@cat $(MAKEFILE_LIST) | grep -E '^[a-zA-Z_-]+:.*?## .*$$' | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

.PHONY: aniva
aniva: ## Build the kernel binary
	make -C ./src/aniva build

.PHONY: drivers
drivers: ## Build on-disk drivers
	@echo -e $(CC) 

.PHONY: libs
libs: ## Build system libraries
	@echo -e "TODO: build libs"

.PHONY: user
user: ## Build userspace stuff
	@echo -e "TODO: build userspace"

.PHONY: ramdisk
ramdisk: ## Create the system ramdisk
	@echo -e "TODO: create ramdisk"

.PHONY: clean
clean: ## Remove any objectfiles
	@echo -e "TODO: clean"

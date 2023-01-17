
# for getting crosscompiler
include ./../../build/cross.mk

# build

HFILES    := $(shell find ./ -type f -name '*.h')
DPEND_FILES := $(patsubst %.h,$(OUT)/%.d,$(HFILES))

CFILES    := $(shell find ./ -type f -name '*.c')
COBJFILES := $(patsubst %.c,$(OUT)/%.o,$(CFILES))

CXXFILES    := $(shell find ./ -type f -name '*.cpp')
CXXOBJFILES := $(patsubst %.cpp,$(OUT)/%.o,$(CXXFILES))

ASMFILES  := $(shell find ./ -type f -name '*.asm')
ASMOBJFILES := $(patsubst %.asm,$(OUT)/%.o,$(ASMFILES))

LINK_PATH := ./src/kernel/linker.ld

# clean

# dump binaries

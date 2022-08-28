#
# Credit goes to WingOS for most of this makefile. Their project has been a great help to mine =)
#

DIRECTORY_GUARD=mkdir -p $(@D)
ARCH :=x86
SRC_PATHS := src libraries/libc libraries/mod libraries/utils
OUT := ./out

HFILES    := $(shell find $(SRC_PATHS) -type f -name '*.h')
DPEND_FILES := $(patsubst %.h,$(OUT)/%.d,$(HFILES))

CFILES    := $(shell find $(SRC_PATHS) -type f -name '*.c')
COBJFILES := $(patsubst %.c,$(OUT)/%.o,$(CFILES))

CXXFILES    := $(shell find $(SRC_PATHS) -type f -name '*.cpp')
CXXOBJFILES := $(patsubst %.cpp,$(OUT)/%.o,$(CXXFILES))

ASMFILES  := $(shell find $(SRC_PATHS) -type f -name '*.asm')
ASMOBJFILES := $(patsubst %.asm,$(OUT)/%.o,$(ASMFILES))

LINK_PATH := ./src/arch/$(ARCH)/linker.ld

NASM	   = /usr/bin/nasm
CC         = ./cross_compiler/bin/x86_64-pc-lightos-gcc
CXX        = ./cross_compiler/bin/x86_64-pc-lightos-g++
LD         = ./cross_compiler/bin/x86_64-pc-lightos-ld

OBJ := $(shell find $(OUT) -type f -name '*.o')

KERNEL_OUT = lightos.elf

# TODO: these flags are also too messy, clean this up too
QEMUFLAGS :=  -m 4G -s -serial stdio -enable-kvm -kernel ./lightos.elf \
		-device VGA,vgamem_mb=64

CHARDFLAGS := $(CFLAGS)               \
        -std=c11                     \
        -fno-pic                       \
        -no-pie \
	    -Wall \
	    -Werror \
        -Os \
		-nostdlib \
        -fno-exceptions \
	    -ffreestanding                 \
        -fno-stack-protector           \
        -fno-omit-frame-pointer        \
	    -fno-isolate-erroneous-paths-attribute \
        -fno-delete-null-pointer-checks \
      	-I./src                        \
        -I./libraries/
 
CXXHARDFLAGS := $(CFLAGS)               \
        -std=c++20                     \
        -g \
        -masm=intel                    \
        -fno-pic                       \
        -no-pie \
        -m64 \
	    -Wall \
	    -MD \
		-msse \
		-mavx \
	    -MMD \
	    -Werror \
        -O3 \
		-nostdlib \
        -mcmodel=kernel \
        -mno-80387                     \
        -mno-red-zone                  \
        -fno-rtti \
        -fno-exceptions \
	    -ffreestanding                 \
        -fno-stack-protector           \
        -fno-omit-frame-pointer        \
	    -fno-isolate-erroneous-paths-attribute \
        -fno-delete-null-pointer-checks \
        -I./src                        \
        -I./libraries/

#-z max-page-size=0x1000
LDHARDFLAGS := $(LDFLAGS)        \
        -T $(LINK_PATH) \
		-Map ./kernel.map


# TODO: this is messy, refactor this.
-include $(DPEND_FILES)
$(OUT)/%.o: %.c 
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (c) $<"
	@$(CC) $(CHARDFLAGS) -c $< -o $@
$(OUT)/%.o: %.cpp 
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (cpp) $<"
	@$(CXX) $(CXXHARDFLAGS) -c $< -o $@
%.h : %.h 
	@echo "[KERNEL $(ARCH)] (h) $<"

$(OUT)/%.o: %.asm
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (asm) $<"
	@$(NASM) $< -o $@ -f elf64

# NOTE: instead of taking all the obj vars indevidually, we might just be able to grab them all from the $(OBJ) variable
.PHONY:$(KERNEL_OUT)
$(KERNEL_OUT): $(COBJFILES) $(CXXOBJFILES) $(ASMOBJFILES) $(LINK_PATH)
	@echo "[LINKING $(ARCH)] $@"
	@ld $(LDHARDFLAGS) $(COBJFILES) $(CXXOBJFILES) $(ASMOBJFILES) -o $@

PHONY:clean
clean:
	-rm -f $(DPEND_FILES)
	-rm -f $(KERNEL_OUT) $(OBJ)

PHONY:run
run:
	@qemu-system-x86_64 $(QEMUFLAGS)

PHONY: make-iso
make-iso: ./lightos.elf grub.cfg
	mkdir -p out/isofiles/boot/grub
	cp grub.cfg out/isofiles/boot/grub
	cp ./lightos.elf out/isofiles/boot
	cp ./kernel.map out/isofiles/boot
	grub-mkrescue -o out/lightos.iso out/isofiles

PHONY: check-multiboot
check-multiboot:
	grub-file --is-x86-multiboot ./$(KERNEL_OUT)

PHONY: check-multiboot2
check-multiboot2:
	grub-file --is-x86-multiboot2 ./$(KERNEL_OUT)

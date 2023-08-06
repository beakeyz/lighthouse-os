DIRECTORY_GUARD=mkdir -p $(@D)
ARCH :=x86
SRC_PATHS := src/aniva src/drivers 
CROSS_PATH := ./cross_compiler/bin
OUT := ./out

HFILES    := $(shell find $(SRC_PATHS) -type f -name '*.h')
DPEND_FILES := $(patsubst %.h,$(OUT)/%.d,$(HFILES))

CFILES    := $(shell find $(SRC_PATHS) -type f -name '*.c')
COBJFILES := $(patsubst %.c,$(OUT)/%.o,$(CFILES))

CXXFILES    := $(shell find $(SRC_PATHS) -type f -name '*.cpp')
CXXOBJFILES := $(patsubst %.cpp,$(OUT)/%.o,$(CXXFILES))

ASMFILES  := $(shell find $(SRC_PATHS) -type f -name '*.asm')
ASMOBJFILES := $(patsubst %.asm,$(OUT)/%.o,$(ASMFILES))

OBJ := $(shell find $(OUT) -type f -name '*.o')

LINK_PATH := ./src/aniva/entry/linker.ld

NASM	   		:= /usr/bin/nasm
CC              := $(CROSS_PATH)/x86_64-pc-lightos-gcc
LD         		:= $(CROSS_PATH)/x86_64-pc-lightos-ld
OC         		:= $(CROSS_PATH)/x86_64-pc-lightos-objcopy

KERNEL_OUT = $(OUT)/lightos.elf

# TODO: these flags are also too messy, clean this up too
# usefull flag for tracing: -d trace:handle_cmd_* 
QEMUFLAGS := -m 128M -machine q35 -usb ./out/lightos.iso -enable-kvm -no-reboot -serial stdio

CHARDFLAGS := -std=gnu11          \
-Wall 							\
-nostdlib						\
-O2 								\
-mno-sse 						\
-mno-sse2						\
-mno-mmx						\
-mno-80387					\
-mno-red-zone				\
-m64 								\
-march=x86-64           \
-mcmodel=large			\
-ffreestanding      \
-fno-stack-protector    \
-fno-stack-check        \
-fshort-wchar           \
-fno-lto                \
-fpie                   \
-fno-exceptions 		\
-MMD								\
-I./src             \
-I./src/aniva/			\

#-z max-page-size=0x1000
LDHARDFLAGS := -T $(LINK_PATH) 						\
							 -Map $(OUT)/lightos.map		\
							 -z max-page-size=0x1000    \

# TODO: this is messy, refactor this.
-include $(DPEND_FILES)
$(OUT)/%.o: %.c 
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (c) $<"
	@$(CC) $(CHARDFLAGS) -c $< -o $@
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
	@$(LD) -o $@ $(COBJFILES) $(CXXOBJFILES) $(ASMOBJFILES) $(LDHARDFLAGS)
	@echo "Done =D"

# TODO: this just builds and runs the kernel for now, but 
# I'd like to have actual debugging capabilities in the future
.PHONY: debug
debug: iso run

.PHONY: clean
clean:
	@echo [CLEAN] Cleaning depends!
	@rm -f $(DPEND_FILES)
	@echo [CLEAN] Cleaning objects!
	@rm -f $(KERNEL_OUT) $(OBJ)
	@rm -rf $(OUT)
	@echo [CLEAN] Done!

.PHONY: build
build: $(KERNEL_OUT)

.PHONY: run
run:
	@qemu-system-x86_64 $(QEMUFLAGS)


.PHONY: iso
iso:
	mkdir -p $(OUT)/isofiles/boot/grub
	cp grub.cfg $(OUT)/isofiles/boot/grub
	cp $(OUT)/lightos.elf $(OUT)/isofiles/boot
	cp $(OUT)/lightos.map $(OUT)/isofiles/boot
	cp $(OUT)/anivaRamdisk.igz $(OUT)/isofiles/boot
	grub-mkrescue -o $(OUT)/lightos.iso $(OUT)/isofiles

.PHONY: check-multiboot
check-multiboot:
	grub-file --is-x86-multiboot ./$(KERNEL_OUT)

.PHONY: check-multiboot2
check-multiboot2:
	grub-file --is-x86-multiboot2 ./$(KERNEL_OUT)

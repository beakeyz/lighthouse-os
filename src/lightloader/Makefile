DIRECTORY_GUARD=mkdir -p $(@D)

override LOADER_LDFLAGS := \
	-Tefi/lib/elf_x86_64_efi.lds \
	-nostdlib \
	-z max-page-size=0x1000 \
	-m elf_x86_64 \
	-static \
	-pie \
	--no-dynamic-linker \
	-z text

override LOADER_CFLAGS := \
	-std=gnu11 \
	-nostdlib \
	-Wall \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fshort-wchar \
	-fno-lto \
	-fPIE \
	-m64 \
	-march=x86-64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-MMD \
	-DGNU_EFI_USE_MS_ABI \
	-I./common/include/ \
	-I./efi/include/ \
	-I./efi/include/x86_64/

SRC_DIRS=common efi
INCLUDE_DIRS=common/include efi/include

# Define the LSDK object
LSDK_OBJ=$(OUT)/lsdk/lsdk.obj

C_SRC := $(shell find $(LOADER_SRC) -type f -name '*.c')
ASM_SRC := $(shell find $(LOADER_SRC) -type f -name '*.asm')
S_SRC := $(shell find $(LOADER_SRC) -type f -name '*.S')

C_OBJ := $(patsubst %.c,%.o,$(subst $(LOADER_SRC),$(LOADER_OUT),$(C_SRC)))
ASM_OBJ := $(patsubst %.asm,%.o,$(subst $(LOADER_SRC),$(LOADER_OUT),$(ASM_SRC)))
S_OBJ := $(patsubst %.S,%.o,$(subst $(LOADER_SRC),$(LOADER_OUT),$(S_SRC)))

override HEADER_DEPS := $(patsubst %.o,%.d,$(C_OBJ))

-include $(HEADER_DEPS)
$(LOADER_OUT)/%.o: $(LOADER_SRC)/%.c
	@echo -e - Building [C]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LOADER_CFLAGS) -c $< -o $@

$(LOADER_OUT)/%.o: $(LOADER_SRC)/%.S
	@echo -e - Building [S]: $@
	@$(DIRECTORY_GUARD)
	@$(CC) $(LOADER_CFLAGS) -c $< -o $@

$(LOADER_OUT)/%.o: $(LOADER_SRC)/%.asm
	@echo -e - Building [ASM]: $@
	@$(DIRECTORY_GUARD)
	@$(ASM) $< -o $@ -f elf64

build: $(C_OBJ) $(ASM_OBJ) $(S_OBJ)
	@echo -e [LOADER] Linking objects...
	@stat $(LSDK_OBJ)
	@$(LD) $^ $(LSDK_OBJ) $(LOADER_LDFLAGS) -o $(LOADER_OUT)/$(LOADER_ELF)
	@$(OBJCOPY) -O binary $(LOADER_OUT)/$(LOADER_ELF) $(LOADER_OUT)/$(LOADER_EFI)
	@echo -e [LOADER] Done =D

.PHONY: build
